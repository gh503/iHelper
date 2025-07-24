#include "CorePlatform/Process.h"
#include "CorePlatform/Windows/WindowsUtils.h"
#include <windows.h>
#include <shellapi.h>
#include <cstring>
#include <stdexcept>
#include <array>
#include <cwchar>
#include <cstdlib>
#include <cctype>
#include <thread>
#include <chrono>
#include <vector>
#include <sstream>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <TlHelp32.h>
#include <iostream>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")

using namespace CorePlatform;

namespace {

HANDLE CreateInheritableHandle() {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;
    return CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, 
                      FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, 
                      OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY, nullptr);
}

bool SetupPipe(HANDLE* readPipe, HANDLE* writePipe, SECURITY_ATTRIBUTES* sa) {
    return CreatePipe(readPipe, writePipe, sa, 0) != FALSE;
}

std::wstring BuildCommandLine(const std::wstring& executable, const std::vector<std::wstring>& args) {
    std::wstring cmdLine;
    // Quote executable if needed
    if (executable.find(L' ') != std::wstring::npos || 
        executable.find(L'\t') != std::wstring::npos) {
        cmdLine += L'"' + executable + L'"';
    } else {
        cmdLine += executable;
    }

    for (const auto& arg : args) {
        cmdLine += L' ';
        if (arg.empty()) {
            cmdLine += L"\"\"";
            continue;
        }
        
        bool needQuote = arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
        if (needQuote) cmdLine += L'"';
        
        for (wchar_t c : arg) {
            if (c == L'"') cmdLine += L'\\';
            cmdLine += c;
        }
        
        if (needQuote) cmdLine += L'"';
    }
    
    return cmdLine;
}

} // namespace

class Process::Impl {
public:
    Impl() : m_process(INVALID_HANDLE_VALUE), m_pid(0), m_exitCode(STILL_ACTIVE) {}
    ~Impl() { Terminate(true); }

    bool Start(const std::string& executable, 
               const std::vector<std::string>& arguments,
               const std::string& workingDirectory,
               const std::vector<std::pair<std::string, std::string>>& environment) {
        
        // Reset previous state
        Terminate(true);

        // Convert to wide strings
        std::wstring wexecutable = WindowsUtils::UTF8ToWide(executable);
        std::wstring wworkdir = WindowsUtils::UTF8ToWide(workingDirectory);
        std::vector<std::wstring> wargs;
        for (const auto& arg : arguments) {
            wargs.push_back(WindowsUtils::UTF8ToWide(arg));
        }

        // Prepare environment block
        wchar_t* envBlock = nullptr;
        if (!environment.empty()) {
            std::wstring envStr;
            for (const auto& [key, value] : environment) {
                std::wstring wkey = WindowsUtils::UTF8ToWide(key);
                std::wstring wval = WindowsUtils::UTF8ToWide(value);
                envStr += wkey + L'=' + wval + L'\0';
            }
            envStr += L'\0'; // Double null-terminate
            envBlock = new wchar_t[envStr.size()];
            wcscpy_s(envBlock, envStr.size(), envStr.c_str());
        }

        // Setup security attributes
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = nullptr;
        sa.bInheritHandle = TRUE;

        // Create pipes
        HANDLE stdinRead = INVALID_HANDLE_VALUE;
        HANDLE stdinWrite = INVALID_HANDLE_VALUE;
        HANDLE stdoutRead = INVALID_HANDLE_VALUE;
        HANDLE stdoutWrite = INVALID_HANDLE_VALUE;
        HANDLE stderrRead = INVALID_HANDLE_VALUE;
        HANDLE stderrWrite = INVALID_HANDLE_VALUE;

        if (!SetupPipe(&stdinRead, &m_stdinWrite, &sa) ||
            !SetupPipe(&m_stdoutRead, &stdoutWrite, &sa) ||
            !SetupPipe(&m_stderrRead, &stderrWrite, &sa)) {
            CleanupHandles(stdinRead, stdinWrite, stdoutRead, stdoutWrite, stderrRead, stderrWrite);
            delete[] envBlock;
            return false;
        }

        // Ensure child doesn't inherit our read end of stdin
        if (!SetHandleInformation(m_stdinWrite, HANDLE_FLAG_INHERIT, 0)) {
            CleanupHandles(stdinRead, stdinWrite, stdoutRead, stdoutWrite, stderrRead, stderrWrite);
            delete[] envBlock;
            return false;
        }

        // Ensure we don't inherit the child's write ends
        SetHandleInformation(m_stdoutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(m_stderrRead, HANDLE_FLAG_INHERIT, 0);

        // Prepare STARTUPINFO
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = stdinRead;
        si.hStdOutput = stdoutWrite;
        si.hStdError = stderrWrite;

        // Prepare process creation
        std::wstring cmdLine = BuildCommandLine(wexecutable, wargs);
        DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;
        if (envBlock) creationFlags |= CREATE_NEW_CONSOLE;

        PROCESS_INFORMATION pi;
        BOOL success = CreateProcessW(
            wexecutable.empty() ? nullptr : wexecutable.c_str(),
            cmdLine.empty() ? nullptr : &cmdLine[0],
            nullptr,
            nullptr,
            TRUE,
            creationFlags,
            envBlock,
            wworkdir.empty() ? nullptr : wworkdir.c_str(),
            &si,
            &pi
        );

        // Clean up handles we don't need
        CloseHandle(stdinRead);
        CloseHandle(stdoutWrite);
        CloseHandle(stderrWrite);
        delete[] envBlock;

        if (!success) {
            CleanupHandles(stdinRead, stdinWrite, stdoutRead, stdoutWrite, stderrRead, stderrWrite);
            return false;
        }

        m_process = pi.hProcess;
        CloseHandle(pi.hThread);
        m_pid = pi.dwProcessId;
        return true;
    }

    bool IsRunning() const {
        if (m_process == INVALID_HANDLE_VALUE) return false;
        DWORD exitCode;
        if (!GetExitCodeProcess(m_process, &exitCode)) return false;
        return exitCode == STILL_ACTIVE;
    }

    bool WaitForFinished(int timeoutMs) {
        if (m_process == INVALID_HANDLE_VALUE) {
            return true;  // 无有效进程
        }

        DWORD waitTime = (timeoutMs == -1) ? INFINITE : static_cast<DWORD>(timeoutMs);
        DWORD result = WaitForSingleObject(m_process, waitTime);

        switch (result) {
            case WAIT_OBJECT_0: {  // 进程已退出
                DWORD exitCode;
                if (!GetExitCodeProcess(m_process, &exitCode)) {
                    // 获取退出码失败
                    exitCode = (DWORD)-1; 
                }
                m_exitCode = exitCode;

                CloseHandle(m_process);  // 必须关闭句柄
                m_process = INVALID_HANDLE_VALUE;
                return true;
            }

            case WAIT_TIMEOUT:  // 超时
                return false;

            case WAIT_FAILED:   // 错误情况
            default: {
                DWORD error = GetLastError();
                // 实际项目中应记录错误日志:
                // std::cerr << "Wait failed: 0x" << std::hex << error << std::dec;
                return false;
            }
        }
    }

    void Terminate(bool force) {
        if (m_process != INVALID_HANDLE_VALUE) {
            TerminateProcess(m_process, force ? 1 : 0);
            WaitForFinished(5000);
            CloseHandle(m_process);
            m_process = INVALID_HANDLE_VALUE;
        }
        CloseHandle(m_stdinWrite);
        CloseHandle(m_stdoutRead);
        CloseHandle(m_stderrRead);
        m_stdinWrite = INVALID_HANDLE_VALUE;
        m_stdoutRead = INVALID_HANDLE_VALUE;
        m_stderrRead = INVALID_HANDLE_VALUE;
    }

    bool WriteInput(const std::string& data) {
        if (m_stdinWrite == INVALID_HANDLE_VALUE) return false;
        DWORD written;
        return WriteFile(m_stdinWrite, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) != 0;
    }

    std::string ReadOutput(int timeoutMs) {
        return ReadFromPipe(m_stdoutRead, timeoutMs);
    }

    std::string ReadError(int timeoutMs) {
        return ReadFromPipe(m_stderrRead, timeoutMs);
    }

    std::optional<int> ExitCode() const {
        if (IsRunning()) return std::nullopt;
        return static_cast<int>(m_exitCode);
    }

    int64_t ProcessId() const {
        return m_pid;
    }

private:
    void CleanupHandles(HANDLE& h1, HANDLE& h2, HANDLE& h3, HANDLE& h4, HANDLE& h5, HANDLE& h6) {
        auto close = [](HANDLE& h) {
            if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
            h = INVALID_HANDLE_VALUE;
        };
        close(h1); close(h2); close(h3); close(h4); close(h5); close(h6);
    }

    std::string ReadFromPipe(HANDLE pipe, int timeoutMs) {
        if (pipe == INVALID_HANDLE_VALUE) return "";
        
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || available == 0) {
            // Handle timeout
            if (timeoutMs > 0) {
                DWORD result = WaitForSingleObject(pipe, timeoutMs);
                if (result != WAIT_OBJECT_0) return "";
                if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || available == 0) {
                    return "";
                }
            } else {
                return "";
            }
        }

        std::string buffer(available, 0);
        DWORD read = 0;
        if (ReadFile(pipe, &buffer[0], available, &read, nullptr)) {
            buffer.resize(read);
            return buffer;
        }
        return "";
    }

    HANDLE m_process;
    HANDLE m_stdinWrite = INVALID_HANDLE_VALUE;
    HANDLE m_stdoutRead = INVALID_HANDLE_VALUE;
    HANDLE m_stderrRead = INVALID_HANDLE_VALUE;
    DWORD m_pid = 0;
    DWORD m_exitCode = STILL_ACTIVE;
};

// Process implementation
Process::Process() : m_impl(std::make_unique<Impl>()) {}
Process::~Process() = default;

bool Process::Start(const std::string& executable, 
                   const std::vector<std::string>& arguments,
                   const std::string& workingDirectory,
                   const std::vector<std::pair<std::string, std::string>>& environment) {
    return m_impl->Start(executable, arguments, workingDirectory, environment);
}

bool Process::IsRunning() const { return m_impl->IsRunning(); }
bool Process::WaitForFinished(int timeoutMs) { return m_impl->WaitForFinished(timeoutMs); }
void Process::Terminate(bool force) { m_impl->Terminate(force); }
bool Process::WriteInput(const std::string& data) { return m_impl->WriteInput(data); }
std::string Process::ReadOutput(int timeoutMs) { return m_impl->ReadOutput(timeoutMs); }
std::string Process::ReadError(int timeoutMs) { return m_impl->ReadError(timeoutMs); }
std::optional<int> Process::ExitCode() const { return m_impl->ExitCode(); }
int64_t Process::ProcessId() const { return m_impl->ProcessId(); }

// Static methods
Process::Result Process::Execute(const std::string& executable, 
                                const std::vector<std::string>& arguments,
                                const std::string& workingDirectory,
                                const std::string& input,
                                const std::vector<std::pair<std::string, std::string>>& environment) {
    Process proc;
    if (!proc.Start(executable, arguments, workingDirectory, environment)) {
        return { -1, "", "Failed to start process" };
    }

    if (!input.empty()) {
        proc.WriteInput(input);
    }

    proc.WaitForFinished(-1);
    return {
        proc.ExitCode().value_or(-1),
        proc.ReadOutput(),
        proc.ReadError()
    };
}

int64_t Process::CurrentProcessId() {
    return static_cast<int64_t>(GetCurrentProcessId());
}

int64_t Process::ParentProcessId() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
    
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    
    DWORD currentPid = GetCurrentProcessId();
    if (Process32First(hSnapshot, &pe)) {
        do {
            if (pe.th32ProcessID == currentPid) {
                CloseHandle(hSnapshot);
                return pe.th32ParentProcessID;
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    
    CloseHandle(hSnapshot);
    return 0;
}

std::string Process::CurrentProcessPath() {
    wchar_t path[MAX_PATH];
    DWORD size = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (size == 0 || size == MAX_PATH) return "";
    return WindowsUtils::WideToUTF8(path);
}