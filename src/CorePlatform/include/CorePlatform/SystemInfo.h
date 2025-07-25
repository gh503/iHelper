#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <chrono>
#include "CorePlatform/Export.h"

namespace CorePlatform {

struct CORE_PLATFORM_API SystemVersion {
    std::string name;        // 操作系统名称 (e.g., "Windows 10", "macOS Monterey")
    std::string version;     // 版本号 (e.g., "10.0.19043", "12.0.1")
    std::string build;       // 构建号 (e.g., "19043", "21A559")
    std::string architecture; // 系统架构 (e.g., "x86_64", "arm64")
};

struct CORE_PLATFORM_API MemoryInfo {
    uint64_t totalPhysical;    // 总物理内存 (bytes)
    uint64_t availablePhysical; // 可用物理内存 (bytes)
    uint64_t totalVirtual;     // 总虚拟内存 (bytes)
    uint64_t availableVirtual; // 可用虚拟内存 (bytes)
};

struct CORE_PLATFORM_API CPUInfo {
    std::string vendor;        // CPU 厂商 (e.g., "GenuineIntel", "AuthenticAMD")
    std::string brand;         // CPU 品牌 (e.g., "Intel Core i7-10700K")
    uint32_t cores;            // 物理核心数
    uint32_t threads;          // 逻辑处理器数
    double clockSpeed;         // 主频 (GHz)
};

struct CORE_PLATFORM_API DiskSpaceInfo {
    uint64_t totalSpace;       // 总空间 (bytes)
    uint64_t freeSpace;        // 可用空间 (bytes)
    uint64_t availableSpace;   // 用户可用空间 (bytes)
};

struct CORE_PLATFORM_API SystemTimeInfo {
    std::chrono::system_clock::time_point bootTime;    // 系统启动时间
    std::chrono::system_clock::time_point currentTime; // 当前系统时间
};

namespace SystemInfo {
    // 获取操作系统信息
    CORE_PLATFORM_API SystemVersion GetOSVersion();
    
    // 获取内存信息
    CORE_PLATFORM_API MemoryInfo GetMemoryInfo();
    
    // 获取CPU信息
    CORE_PLATFORM_API CPUInfo GetCPUInfo();
    
    // 获取系统启动时间
    CORE_PLATFORM_API std::chrono::system_clock::time_point GetBootTime();
    
    // 获取系统运行时间
    CORE_PLATFORM_API std::chrono::milliseconds GetUptime();
    
    // 获取系统时间信息
    CORE_PLATFORM_API SystemTimeInfo GetSystemTimeInfo();
    
    // 获取用户名
    CORE_PLATFORM_API std::string GetUsername();
    
    // 检查当前用户是否为管理员
    CORE_PLATFORM_API bool IsAdmin();
    
    // 获取磁盘空间信息
    CORE_PLATFORM_API DiskSpaceInfo GetDiskSpace(const std::string& path);
    
    // 获取所有挂载点信息 (仅限macOS/Linux)
    CORE_PLATFORM_API std::vector<std::string> GetMountPoints();
};

} // namespace CorePlatform