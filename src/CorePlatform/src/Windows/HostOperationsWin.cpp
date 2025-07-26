#include "CorePlatform/HostOperations.h"
#include "CorePlatform/Windows/WindowsUtils.h"
#include <Windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <processthreadsapi.h>
#include <winsvc.h>
#include <sddl.h> // ConvertSidToStringSid
#include <sstream>
#include <memory>
#include <vector>
#include <algorithm>
#include <winternl.h> // 包含 NTSTATUS 和 PROCESS_BASIC_INFORMATION 定义
#pragma comment(lib, "ntdll.lib") // 链接 ntdll.lib

namespace CorePlatform {

// 获取Windows错误码
static std::error_code GetWinError() {
    return std::error_code(static_cast<int>(GetLastError()), std::system_category());
}

// 进程操作实现
void StartProcess(const std::string& path, const std::vector<std::string>& args) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    std::wstring commandLine = L"\"" + wpath + L"\"";
    
    for (const auto& arg : args) {
        commandLine += L" \"" + WindowsUtils::UTF8ToWide(arg) + L"\"";
    }
    
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {nullptr, nullptr, 0, 0};
    
    if (!CreateProcessW(
        nullptr, 
        &commandLine[0],
        nullptr, 
        nullptr, 
        FALSE, 
        0, 
        nullptr, 
        nullptr, 
        &si, 
        &pi)) 
    {
        throw HostException(GetWinError(), "CreateProcess failed: " + WindowsUtils::GetLastErrorString());
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void TerminateProcess(int pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!hProcess) {
        throw HostException(GetWinError(), "OpenProcess failed: " + WindowsUtils::GetLastErrorString());
    }
    
    if (!::TerminateProcess(hProcess, 1)) {
        CloseHandle(hProcess);
        throw HostException(GetWinError(), "TerminateProcess failed" + WindowsUtils::GetLastErrorString());
    }
    
    CloseHandle(hProcess);
}

ProcessInfo GetProcessInfo(int pid) {
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
        FALSE, 
        static_cast<DWORD>(pid)
    );
    
    if (!hProcess) {
        throw HostException(GetWinError(), "OpenProcess failed");
    }
    
    ProcessInfo info;
    info.pid = pid;
    
    // 获取进程名
    WCHAR exePath[MAX_PATH] = {0};
    if (GetProcessImageFileNameW(hProcess, exePath, MAX_PATH) > 0) {
        info.name = WindowsUtils::WideToUTF8(exePath);
    }
    
    // 获取内存使用
    PROCESS_MEMORY_COUNTERS pmc;
    memset(&pmc, 0, sizeof(pmc));
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        info.memoryUsage = pmc.WorkingSetSize / 1024;
    }
    
    // 获取启动时间
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER uli;
        uli.LowPart = createTime.dwLowDateTime;
        uli.HighPart = createTime.dwHighDateTime;
        info.startTime = uli.QuadPart / 10000000ULL - 11644473600ULL; // 转换为Unix时间戳
    }
    
    // 获取命令行 - 使用更稳定的方法
    DWORD bufferSize = 0;
    std::vector<WCHAR> commandLineBuffer;
    
    // 第一次调用获取所需缓冲区大小
    QueryFullProcessImageNameW(hProcess, 0, nullptr, &bufferSize);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        commandLineBuffer.resize(bufferSize);
        if (QueryFullProcessImageNameW(hProcess, 0, commandLineBuffer.data(), &bufferSize)) {
            info.commandLine = WindowsUtils::WideToUTF8(commandLineBuffer.data());
        }
    }
    
    // 获取所有者
    HANDLE hToken = nullptr;
    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        DWORD tokenSize = 0;
        GetTokenInformation(hToken, TokenUser, nullptr, 0, &tokenSize);
        if (tokenSize > 0) {
            std::vector<BYTE> tokenBuf(tokenSize);
            if (GetTokenInformation(hToken, TokenUser, tokenBuf.data(), tokenSize, &tokenSize)) {
                PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(tokenBuf.data());
                LPWSTR sidString = nullptr;
                if (ConvertSidToStringSidW(pTokenUser->User.Sid, &sidString)) {
                    info.owner = WindowsUtils::WideToUTF8(sidString);
                    LocalFree(sidString);
                }
            }
        }
        CloseHandle(hToken);
    }
    
    CloseHandle(hProcess);
    return info;
}

std::vector<ProcessInfo> ListProcesses() {
    std::vector<ProcessInfo> processes;
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        throw HostException(GetWinError(), "CreateToolhelp32Snapshot failed: " + WindowsUtils::GetLastErrorString());
    }
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    if (!Process32FirstW(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        throw HostException(GetWinError(), "Process32First failed: " + WindowsUtils::GetLastErrorString());
    }
    
    do {
        ProcessInfo info;
        info.pid = pe32.th32ProcessID;
        info.name = WindowsUtils::WideToUTF8(pe32.szExeFile);
        processes.push_back(info);
    } while (Process32NextW(hSnapshot, &pe32));
    
    CloseHandle(hSnapshot);
    return processes;
}

// 服务操作实现
void InstallService(const std::string& name, 
                   const std::string& displayName,
                   const std::string& binPath) 
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::wstring wname = WindowsUtils::UTF8ToWide(name);
    std::wstring wdisplay = WindowsUtils::UTF8ToWide(displayName);
    std::wstring wpath = WindowsUtils::UTF8ToWide(binPath);
    
    SC_HANDLE service = CreateServiceW(
        scm,
        wname.c_str(),
        wdisplay.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        wpath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
    
    if (!service) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "CreateService failed: " + WindowsUtils::GetLastErrorString(GetLastError()));
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

void UninstallService(const std::string& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::wstring wname = WindowsUtils::UTF8ToWide(name);
    SC_HANDLE service = OpenServiceW(scm, wname.c_str(), 
                                    DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    
    if (!service) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "OpenService failed: " + WindowsUtils::GetLastErrorString());
    }
    
    // 停止服务
    SERVICE_STATUS status = {0, 0, 0, 0, 0, 0, 0};
    if (ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        // 等待服务停止
        for (int i = 0; i < 10; i++) {
            if (!QueryServiceStatus(service, &status)) break;
            if (status.dwCurrentState == SERVICE_STOPPED) break;
            Sleep(500);
        }
    }
    
    if (!DeleteService(service)) {
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "DeleteService failed: " + WindowsUtils::GetLastErrorString(GetLastError()));
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

void StartService(const std::string& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::wstring wname = WindowsUtils::UTF8ToWide(name);
    SC_HANDLE service = OpenServiceW(scm, wname.c_str(), SERVICE_START);
    
    if (!service) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "OpenService failed: " + WindowsUtils::GetLastErrorString());
    }
    
    if (!::StartServiceW(service, 0, nullptr)) {
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "StartService failed: " + WindowsUtils::GetLastErrorString(GetLastError()));
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

void StopService(const std::string& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::wstring wname = WindowsUtils::UTF8ToWide(name);
    SC_HANDLE service = OpenServiceW(scm, wname.c_str(), SERVICE_STOP);
    
    if (!service) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "OpenService failed: " + WindowsUtils::GetLastErrorString());
    }
    
    SERVICE_STATUS status = {0, 0, 0, 0, 0, 0, 0};
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "ControlService failed: " + WindowsUtils::GetLastErrorString(GetLastError()));
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

ServiceInfo GetServiceInfo(const std::string& name) {
    ServiceInfo info;
    info.name = name;
    
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::wstring wname = WindowsUtils::UTF8ToWide(name);
    SC_HANDLE service = OpenServiceW(scm, wname.c_str(), 
                                    SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    
    if (!service) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "OpenService failed: " + WindowsUtils::GetLastErrorString());
    }
    
    // 获取服务状态
    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded;
    if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, 
                           reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &bytesNeeded)) 
    {
        switch (ssp.dwCurrentState) {
            case SERVICE_STOPPED: info.status = ServiceStatus::Stopped; break;
            case SERVICE_START_PENDING: info.status = ServiceStatus::StartPending; break;
            case SERVICE_RUNNING: info.status = ServiceStatus::Running; break;
            case SERVICE_STOP_PENDING: info.status = ServiceStatus::StopPending; break;
            default: info.status = ServiceStatus::Unknown;
        }
    }
    
    // 获取服务配置
    DWORD bufSize = 0;
    QueryServiceConfigW(service, nullptr, 0, &bufSize);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<BYTE> buffer(bufSize);
        LPQUERY_SERVICE_CONFIGW config = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(buffer.data());
        if (QueryServiceConfigW(service, config, bufSize, &bufSize)) {
            info.displayName = WindowsUtils::WideToUTF8(config->lpDisplayName);
            info.binaryPath = WindowsUtils::WideToUTF8(config->lpBinaryPathName);
            info.isAutoStart = (config->dwStartType == SERVICE_AUTO_START);
        }
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return info;
}

std::vector<ServiceInfo> ListServices() {
    std::vector<ServiceInfo> services;
    
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, 
                                  SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    DWORD bufSize = 0;
    DWORD servicesReturned = 0;
    DWORD resumeHandle = 0;
    
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
                         nullptr, 0, &bufSize, &servicesReturned, &resumeHandle, nullptr);
    
    if (GetLastError() != ERROR_MORE_DATA) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "EnumServicesStatusEx failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::vector<BYTE> buffer(bufSize);
    ENUM_SERVICE_STATUS_PROCESSW* serviceStatus = 
        reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
    
    if (!EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
                             buffer.data(), bufSize, &bufSize, &servicesReturned, 
                             &resumeHandle, nullptr)) 
    {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "EnumServicesStatusEx failed: " + WindowsUtils::GetLastErrorString());
    }
    
    for (DWORD i = 0; i < servicesReturned; i++) {
        ServiceInfo info;
        info.name = WindowsUtils::WideToUTF8(serviceStatus[i].lpServiceName);
        info.displayName = WindowsUtils::WideToUTF8(serviceStatus[i].lpDisplayName);
        
        switch (serviceStatus[i].ServiceStatusProcess.dwCurrentState) {
            case SERVICE_STOPPED: info.status = ServiceStatus::Stopped; break;
            case SERVICE_START_PENDING: info.status = ServiceStatus::StartPending; break;
            case SERVICE_RUNNING: info.status = ServiceStatus::Running; break;
            case SERVICE_STOP_PENDING: info.status = ServiceStatus::StopPending; break;
            default: info.status = ServiceStatus::Unknown;
        }
        
        services.push_back(info);
    }
    
    CloseServiceHandle(scm);
    return services;
}

// 驱动操作实现
void InstallDriver(const std::string& name, const std::string& filePath) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::wstring wname = WindowsUtils::UTF8ToWide(name);
    std::wstring wpath = WindowsUtils::UTF8ToWide(filePath);
    
    SC_HANDLE service = CreateServiceW(
        scm,
        wname.c_str(),
        wname.c_str(), // 显示名与名称相同
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        wpath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
    
    if (!service) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "CreateService failed for driver: " + WindowsUtils::GetLastErrorString(GetLastError()));
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

void UninstallDriver(const std::string& name) {
    // 先停止驱动
    try {
        UnloadDriver(name);
    } catch (...) {
        // 忽略停止错误，继续卸载
    }
    
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::wstring wname = WindowsUtils::UTF8ToWide(name);
    SC_HANDLE service = OpenServiceW(scm, wname.c_str(), DELETE);
    
    if (!service) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "OpenService failed: " + WindowsUtils::GetLastErrorString());
    }
    
    if (!DeleteService(service)) {
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "DeleteService failed: " + WindowsUtils::GetLastErrorString(GetLastError()));
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

void LoadDriver(const std::string& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::wstring wname = WindowsUtils::UTF8ToWide(name);
    SC_HANDLE service = OpenServiceW(scm, wname.c_str(), SERVICE_START);
    
    if (!service) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "OpenService failed: " + WindowsUtils::GetLastErrorString());
    }
    
    if (!StartServiceW(service, 0, nullptr)) {
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "StartService failed for driver: " + WindowsUtils::GetLastErrorString(GetLastError()));
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

void UnloadDriver(const std::string& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::wstring wname = WindowsUtils::UTF8ToWide(name);
    SC_HANDLE service = OpenServiceW(scm, wname.c_str(), SERVICE_STOP);
    
    if (!service) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "OpenService failed: " + WindowsUtils::GetLastErrorString());
    }
    
    SERVICE_STATUS status;
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "ControlService failed: " + WindowsUtils::GetLastErrorString(GetLastError()));
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
}

DriverInfo GetDriverInfo(const std::string& name) {
    DriverInfo info;
    info.name = name;
    
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        throw HostException(GetWinError(), "OpenSCManager failed: " + WindowsUtils::GetLastErrorString());
    }
    
    std::wstring wname = WindowsUtils::UTF8ToWide(name);
    SC_HANDLE service = OpenServiceW(scm, wname.c_str(), SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    
    if (!service) {
        CloseServiceHandle(scm);
        throw HostException(GetWinError(), "OpenService failed: " + WindowsUtils::GetLastErrorString());
    }
    
    // 获取驱动状态
    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded;
    if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, 
                           reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &bytesNeeded)) 
    {
        switch (ssp.dwCurrentState) {
            case SERVICE_STOPPED: info.status = DriverStatus::Stopped; break;
            case SERVICE_RUNNING: info.status = DriverStatus::Running; break;
            default: info.status = DriverStatus::Unknown;
        }
    }
    
    // 获取驱动路径
    DWORD bufSize = 0;
    QueryServiceConfigW(service, nullptr, 0, &bufSize);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<BYTE> buffer(bufSize);
        LPQUERY_SERVICE_CONFIGW config = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(buffer.data());
        if (QueryServiceConfigW(service, config, bufSize, &bufSize)) {
            info.filePath = WindowsUtils::WideToUTF8(config->lpBinaryPathName);
        }
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return info;
}

} // namespace CorePlatform