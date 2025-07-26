#pragma once
#include <string>
#include <vector>
#include <system_error>
#include <stdexcept>
#include <cstdint>
#include <ctime>
#include "CorePlatform/Export.h"

namespace CorePlatform {

// 异常类
class HostException : public std::system_error {
public:
    using std::system_error::system_error;
};

// 进程信息结构
struct CORE_PLATFORM_API ProcessInfo {
    int pid = 0;
    std::string name;
    std::string owner;
    uint64_t memoryUsage = 0;  // KB
    std::time_t startTime = 0;
    std::string commandLine;
};

// 服务状态枚举
enum class ServiceStatus {
    Stopped,
    StartPending,
    Running,
    StopPending,
    Unknown
};

// 服务信息结构
struct CORE_PLATFORM_API ServiceInfo {
    std::string name;
    std::string displayName;
    ServiceStatus status = ServiceStatus::Unknown;
    std::string binaryPath;
    bool isAutoStart = false;
};

// 驱动状态枚举
enum class DriverStatus {
    Stopped,
    Running,
    BootLoaded,
    Unknown
};

// 驱动信息结构
struct CORE_PLATFORM_API DriverInfo {
    std::string name;
    std::string filePath;
    DriverStatus status = DriverStatus::Unknown;
    int loadOrder = 0;
};

// ====== 进程操作 ======
void CORE_PLATFORM_API StartProcess(const std::string& path, 
                 const std::vector<std::string>& args = {});

void CORE_PLATFORM_API TerminateProcess(int pid);
ProcessInfo CORE_PLATFORM_API GetProcessInfo(int pid);
std::vector<ProcessInfo> CORE_PLATFORM_API ListProcesses();

// ====== 服务操作 ======
void CORE_PLATFORM_API InstallService(const std::string& name, 
                   const std::string& displayName,
                   const std::string& binPath);

void CORE_PLATFORM_API UninstallService(const std::string& name);
void CORE_PLATFORM_API StartService(const std::string& name);
void CORE_PLATFORM_API StopService(const std::string& name);
ServiceInfo CORE_PLATFORM_API GetServiceInfo(const std::string& name);
std::vector<ServiceInfo> CORE_PLATFORM_API ListServices();

// ====== 驱动操作 ======
void CORE_PLATFORM_API InstallDriver(const std::string& name, 
                  const std::string& filePath);

void CORE_PLATFORM_API UninstallDriver(const std::string& name);
void CORE_PLATFORM_API LoadDriver(const std::string& name);
void CORE_PLATFORM_API UnloadDriver(const std::string& name);
DriverInfo CORE_PLATFORM_API GetDriverInfo(const std::string& name);

} // namespace CorePlatform