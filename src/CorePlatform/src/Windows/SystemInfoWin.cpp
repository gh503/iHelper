#include "CorePlatform/SystemInfo.h"
#include "CorePlatform/Windows/WindowsUtils.h"
#include <windows.h>
#include <sysinfoapi.h>
#include <lmcons.h>
#include <winbase.h>
#include <winternl.h>
#include <intrin.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <iostream>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "advapi32.lib")

namespace CorePlatform {

// 获取注册表字符串值（处理32/64位重定向）
static std::wstring GetRegistryString(HKEY hKey, const wchar_t* subKey, const wchar_t* valueName) {
    HKEY hSubKey;
    REGSAM access = KEY_READ;
    
    // 如果是64位系统上的32位进程，需要特殊处理
    BOOL isWow64 = FALSE;
    if (IsWow64Process(GetCurrentProcess(), &isWow64)) {
        if (isWow64) {
            // 32位进程在64位系统上运行
            access |= KEY_WOW64_64KEY;
        }
    }
    
    if (RegOpenKeyExW(hKey, subKey, 0, access, &hSubKey) != ERROR_SUCCESS) {
        return L"";
    }
    
    DWORD dataSize;
    if (RegQueryValueExW(hSubKey, valueName, NULL, NULL, NULL, &dataSize) != ERROR_SUCCESS) {
        RegCloseKey(hSubKey);
        return L"";
    }
    
    std::wstring buffer(dataSize / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(hSubKey, valueName, NULL, NULL, 
                         reinterpret_cast<LPBYTE>(&buffer[0]), &dataSize) != ERROR_SUCCESS) {
        RegCloseKey(hSubKey);
        return L"";
    }
    
    RegCloseKey(hSubKey);
    
    // 移除可能的空字符结尾
    if (!buffer.empty() && buffer.back() == L'\0') {
        buffer.pop_back();
    }
    return buffer;
}

SystemVersion SystemInfo::GetOSVersion() {
    SystemVersion version;
    
    // 优先从注册表获取详细操作系统名称
    std::wstring productName = GetRegistryString(
        HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
        L"ProductName"
    );
    version.name = WindowsUtils::WideToUTF8(productName);
    
    // 使用API获取版本号信息
    OSVERSIONINFOEXW osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
    memset(&osvi, 0, sizeof(osvi));
    NTSTATUS(WINAPI *RtlGetVersion)(LPOSVERSIONINFOEXW) = nullptr;
    
    *(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    
    if (RtlGetVersion && RtlGetVersion(&osvi) == 0) {
        // 构建完整版本号字符串
        std::ostringstream ss;
        ss << osvi.dwMajorVersion << "." << osvi.dwMinorVersion << "." << osvi.dwBuildNumber;
        version.version = ss.str();
        
        // 特殊处理：识别Windows 11
        if (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 22000) {
            // 保留注册表名称，但如果是"Windows 10"则升级显示
            if (version.name.find("Windows 10") != std::string::npos) {
                version.name = "Windows 11" + version.name.substr(10);
            }
        }
    } else {
        // API失败时的备选方案
        version.version = "0.0.0";
    }
    
    // 获取构建号（直接从注册表获取）
    std::wstring buildNumber = GetRegistryString(
        HKEY_LOCAL_MACHINE, 
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
        L"CurrentBuildNumber"
    );
    version.build = WindowsUtils::WideToUTF8(buildNumber);
    
    // 获取系统架构（保持不变）
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            version.architecture = "x86_64";
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            version.architecture = "ARM";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            version.architecture = "ARM64";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            version.architecture = "x86";
            break;
        default:
            version.architecture = "Unknown";
    }
    
    return version;
}

MemoryInfo SystemInfo::GetMemoryInfo() {
    MemoryInfo info = {0, 0, 0, 0};
    
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        info.totalPhysical = memStatus.ullTotalPhys;
        info.availablePhysical = memStatus.ullAvailPhys;
        info.totalVirtual = memStatus.ullTotalVirtual;
        info.availableVirtual = memStatus.ullAvailVirtual;
    }
    
    return info;
}

CPUInfo SystemInfo::GetCPUInfo() {
    CPUInfo info = {"", "", 0, 0, 0.0};
    
    int cpuInfo[4] = {-1};
    char vendor[13] = {0};
    
    // 获取CPU厂商
    __cpuid(cpuInfo, 0);
    memcpy(vendor, &cpuInfo[1], 4);
    memcpy(vendor + 4, &cpuInfo[3], 4);
    memcpy(vendor + 8, &cpuInfo[2], 4);
    info.vendor = vendor;
    
    // 获取CPU品牌
    char brand[0x40] = {0};
    __cpuid(cpuInfo, 0x80000000);
    if (static_cast<uint32_t>(cpuInfo[0]) >= 0x80000004) {
        __cpuid(reinterpret_cast<int*>(brand), 0x80000002);
        __cpuid(reinterpret_cast<int*>(brand + 16), 0x80000003);
        __cpuid(reinterpret_cast<int*>(brand + 32), 0x80000004);
        info.brand = brand;
    }
    
    // 获取CPU核心和线程数
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info.cores = sysInfo.dwNumberOfProcessors;
    
    // 获取逻辑处理器数
    DWORD bufferSize = 0;
    GetLogicalProcessorInformationEx(RelationAll, nullptr, &bufferSize);
    std::vector<BYTE> buffer(bufferSize);
    if (GetLogicalProcessorInformationEx(RelationAll, 
                                        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), 
                                        &bufferSize)) {
        BYTE* ptr = buffer.data();
        while (ptr < buffer.data() + bufferSize) {
            auto infoEx = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(ptr);
            if (infoEx->Relationship == RelationProcessorCore) {
                info.threads += infoEx->Processor.Flags == LTP_PC_SMT ? 2 : 1;
            }
            ptr += infoEx->Size;
        }
    } else {
        info.threads = info.cores;
    }
    
    // 获取CPU频率 - 使用API替代注册表查询
    HQUERY hQuery;
    HCOUNTER hCounter;
    PDH_FMT_COUNTERVALUE counterValue;
    
    if (PdhOpenQueryW(nullptr, 0, &hQuery) == ERROR_SUCCESS) {
        if (PdhAddCounterW(hQuery, L"\\Processor Information(_Total)\\% Processor Frequency", 0, &hCounter) == ERROR_SUCCESS) {
            if (PdhCollectQueryData(hQuery) == ERROR_SUCCESS) {
                if (PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, nullptr, &counterValue) == ERROR_SUCCESS) {
                    info.clockSpeed = counterValue.doubleValue / 1000.0; // MHz to GHz
                }
            }
        }
        PdhCloseQuery(hQuery);
    }
    
    // 如果API失败，使用备用方法
    if (info.clockSpeed == 0.0) {
        DWORD mhz;
        DWORD size = sizeof(mhz);
        HKEY hKey;
        
        // 使用处理重定向的注册表访问
        REGSAM access = KEY_READ;
        BOOL isWow64 = FALSE;
        if (IsWow64Process(GetCurrentProcess(), &isWow64) && isWow64) {
            access |= KEY_WOW64_64KEY;
        }
        
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                          L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 
                          0, access, &hKey) == ERROR_SUCCESS) {
            if (RegQueryValueExW(hKey, L"~MHz", NULL, NULL, 
                                 reinterpret_cast<LPBYTE>(&mhz), &size) == ERROR_SUCCESS) {
                info.clockSpeed = mhz / 1000.0;
            }
            RegCloseKey(hKey);
        }
    }
    
    return info;
}

std::chrono::system_clock::time_point SystemInfo::GetBootTime() {
    ULONGLONG uptime = GetTickCount64();
    auto now = std::chrono::system_clock::now();
    return now - std::chrono::milliseconds(uptime);
}

std::chrono::milliseconds SystemInfo::GetUptime() {
    return std::chrono::milliseconds(GetTickCount64());
}

SystemTimeInfo SystemInfo::GetSystemTimeInfo() {
    SystemTimeInfo info;
    info.currentTime = std::chrono::system_clock::now();
    info.bootTime = GetBootTime();
    return info;
}

std::string SystemInfo::GetUsername() {
    wchar_t username[UNLEN + 1];
    DWORD size = UNLEN + 1;
    if (GetUserNameW(username, &size)) {
        return WindowsUtils::WideToUTF8(username);
    }
    return "";
}

bool SystemInfo::IsAdmin() {
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    
    if (!AllocateAndInitializeSid(&NtAuthority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &AdministratorsGroup)) {
        return false;
    }
    
    BOOL isMember;
    if (!CheckTokenMembership(NULL, AdministratorsGroup, &isMember)) {
        FreeSid(AdministratorsGroup);
        return false;
    }
    
    FreeSid(AdministratorsGroup);
    return isMember != FALSE;
}

DiskSpaceInfo SystemInfo::GetDiskSpace(const std::string& path) {
    DiskSpaceInfo info = {0, 0, 0};
    
    ULARGE_INTEGER totalBytes;
    ULARGE_INTEGER freeBytes;
    ULARGE_INTEGER availableBytes;
    
    if (GetDiskFreeSpaceExW(std::wstring(path.begin(), path.end()).c_str(),
                            &availableBytes, &totalBytes, &freeBytes)) {
        info.totalSpace = totalBytes.QuadPart;
        info.freeSpace = freeBytes.QuadPart;
        info.availableSpace = availableBytes.QuadPart;
    }
    
    return info;
}

std::vector<std::string> SystemInfo::GetMountPoints() {
    // Windows没有挂载点的概念，返回驱动器列表
    std::vector<std::string> drives;
    
    DWORD driveMask = GetLogicalDrives();
    for (char drive = 'A'; drive <= 'Z'; drive++) {
        if (driveMask & 1) {
            drives.push_back(std::string(1, drive) + ":\\");
        }
        driveMask >>= 1;
    }
    
    return drives;
}

} // namespace CorePlatform