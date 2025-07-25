#include "CorePlatform/Process.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <climits>
#include <filesystem>
#include <fstream>

using namespace CorePlatform;

namespace {

std::vector<char*> StringVectorToArgv(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

std::vector<std::string> GetEnvironmentVector(
    const std::vector<std::pair<std::string, std::string>>& env) {
    
    std::vector<std::string> envVec;
    if (env.empty()) {
        // Copy current environment
        for (char** envp = environ; *envp != nullptr; envp++) {
            envVec.push_back(*envp);
        }
    } else {
        for (const auto& [key, value] : env) {
            envVec.push_back(key + "=" + value);
        }
    }
    return envVec;
}

std::vector<char*> GetEnvironmentPointers(
    const std::vector<std::string>& envVec) {
    
    std::vector<char*> envp;
    envp.reserve(envVec.size() + 1);
    for (const auto& e : envVec) {
        envp.push_back(const_cast<char*>(e.c_str()));
    }
    envp.push_back(nullptr);
    return envp;
}

std::string ReadFromFd(int fd, int timeoutMs) {
    if (fd < 0) return "";
    
    fd_set set;
    struct timeval timeout;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    
    if (select(fd + 1, &set, nullptr, nullptr, timeoutMs >= 0 ? &timeout : nullptr) <= 0) {
        return "";
    }
    
    std::array<char, 4096> buffer;
    std::string result;
    ssize_t count = 0;
    while ((count = read(fd, buffer.data(), buffer.size()))) {
        if (count < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            break;
        }
        result.append(buffer.data(), count);
    }
    return result;
}

} // namespace

class Process::Impl {
public:
    Impl() : m_pid(-1), m_exitCode(-1), m_stdin(-1), m_stdout(-1), m_stderr(-1) {}
    ~Impl() { Terminate(true); }

    bool Start(const std::string& executable, 
               const std::vector<std::string>& arguments,
               const std::string& workingDirectory,
               const std::vector<std::pair<std::string, std::string>>& environment) {
        
        // Reset previous state
        Terminate(true);

        // Create pipes
        int stdinPipe[2] = {-1, -1};
        int stdoutPipe[2] = {-1, -1};
        int stderrPipe[2] = {-1, -1};
        
        if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0) {
            ClosePipes(stdinPipe, stdoutPipe, stderrPipe);
            return false;
        }

        m_pid = fork();
        if (m_pid == -1) {
            ClosePipes(stdinPipe, stdoutPipe, stderrPipe);
            return false;
        }

        if (m_pid == 0) { // Child process
            close(stdinPipe[1]); // Close write end of stdin
            close(stdoutPipe[0]); // Close read end of stdout
            close(stderrPipe[0]); // Close read end of stderr

            // Redirect standard streams
            if (dup2(stdinPipe[0], STDIN_FILENO) == -1) exit(127);
            if (dup2(stdoutPipe[1], STDOUT_FILENO) == -1) exit(127);
            if (dup2(stderrPipe[1], STDERR_FILENO) == -1) exit(127);

            // Close unused file descriptors
            close(stdinPipe[0]);
            close(stdoutPipe[1]);
            close(stderrPipe[1]);

            // Set working directory
            if (!workingDirectory.empty()) {
                if (chdir(workingDirectory.c_str()) != 0) exit(127);
            }

            // Prepare arguments
            std::vector<char*> argv = StringVectorToArgv(arguments);
            argv.insert(argv.begin(), const_cast<char*>(executable.c_str()));

            // Prepare environment
            auto envVec = GetEnvironmentVector(environment);
            auto envp = GetEnvironmentPointers(envVec);

            // Execute the process
            execvpe(executable.c_str(), argv.data(), envp.data());
            
            // If we get here, exec failed
            exit(127);
        } else { // Parent process
            close(stdinPipe[0]); // Close read end of stdin
            close(stdoutPipe[1]); // Close write end of stdout
            close(stderrPipe[1]); // Close write end of stderr

            m_stdin = stdinPipe[1];
            m_stdout = stdoutPipe[0];
            m_stderr = stderrPipe[0];
            
            // Set non-blocking mode for stdout/stderr
            int flags = fcntl(m_stdout, F_GETFL, 0);
            fcntl(m_stdout, F_SETFL, flags | O_NONBLOCK);
            flags = fcntl(m_stderr, F_GETFL, 0);
            fcntl(m_stderr, F_SETFL, flags | O_NONBLOCK);
            
            return true;
        }
    }

    bool IsRunning() const {
        if (m_pid <= 0) return false;
        int status;
        pid_t result = waitpid(m_pid, &status, WNOHANG);
        if (result == 0) return true;
        if (result == -1) return false;
        return false;
    }

    bool WaitForFinished(int timeoutMs) {
        if (m_pid <= 0) return true;
        
        if (timeoutMs == -1) {
            int status;
            waitpid(m_pid, &status, 0);
            m_exitCode = WEXITSTATUS(status);
            return true;
        }
        
        auto start = std::chrono::steady_clock::now();
        while (true) {
            int status;
            pid_t result = waitpid(m_pid, &status, WNOHANG);
            if (result != 0) {
                if (result > 0) m_exitCode = WEXITSTATUS(status);
                return result > 0;
            }
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed >= timeoutMs) return false;
            
            usleep(10000); // 10ms
        }
    }

    void Terminate(bool force) {
        if (m_pid > 0) {
            kill(m_pid, force ? SIGKILL : SIGTERM);
            waitpid(m_pid, nullptr, 0);
            m_pid = -1;
        }
        CloseAllPipes();
    }

    bool WriteInput(const std::string& data) {
        if (m_stdin < 0) return false;
        ssize_t written = write(m_stdin, data.data(), data.size());
        if (written < 0) return false;
        if (static_cast<size_t>(written) < data.size()) {
            // Partial write, try again with remaining
            return WriteInput(data.substr(written));
        }
        return true;
    }

    std::string ReadOutput(int timeoutMs) {
        return ReadFromFd(m_stdout, timeoutMs);
    }

    std::string ReadError(int timeoutMs) {
        return ReadFromFd(m_stderr, timeoutMs);
    }

    std::optional<int> ExitCode() const {
        if (IsRunning()) return std::nullopt;
        return m_exitCode;
    }

    int64_t ProcessId() const {
        return static_cast<int64_t>(m_pid);
    }

private:
    void ClosePipes(int (&stdin)[2], int (&stdout)[2], int (&stderr)[2]) {
        auto closeFd = [](int fd) { if (fd >= 0) close(fd); };
        closeFd(stdin[0]); closeFd(stdin[1]);
        closeFd(stdout[0]); closeFd(stdout[1]);
        closeFd(stderr[0]); closeFd(stderr[1]);
    }
    
    void CloseAllPipes() {
        auto closeFd = [](int& fd) { if (fd >= 0) close(fd); fd = -1; };
        closeFd(m_stdin);
        closeFd(m_stdout);
        closeFd(m_stderr);
    }

    pid_t m_pid;
    int m_exitCode;
    int m_stdin;
    int m_stdout;
    int m_stderr;
};

// Process implementation (same as Windows)
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
    return static_cast<int64_t>(getpid());
}

int64_t Process::ParentProcessId() {
    return static_cast<int64_t>(getppid());
}

std::string Process::CurrentProcessPath() {
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path)-1);
    if (count == -1) return "";
    path[count] = '\0';
    return path;
}