#include "CorePlatform/SystemInfo.h"
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <pwd.h>
#include <sys/statvfs.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <fstream>

namespace CorePlatform {

static std::string GetSysCtlString(const char* name) {
    size_t size;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0) {
        return "";
    }
    
    std::string buffer(size, '\0');
    if (sysctlbyname(name, &buffer[0], &size, nullptr, 0) != 0) {
        return "";
    }
    
    // 移除可能的空字符结尾
    if (!buffer.empty() && buffer.back() == '\0') {
        buffer.pop_back();
    }
    return buffer;
}

static uint64_t GetSysCtlUInt64(const char* name) {
    uint64_t value;
    size_t size = sizeof(value);
    if (sysctlbyname(name, &value, &size, nullptr, 0) != 0) {
        return 0;
    }
    return value;
}

SystemVersion SystemInfo::GetOSVersion() {
    SystemVersion version;
    
    // 获取产品名称
    version.name = GetSysCtlString("kern.ostype");
    
    // 获取版本信息
    char release[256];
    size_t size = sizeof(release);
    if (sysctlbyname("kern.osrelease", release, &size, nullptr, 0) == 0) {
        version.version = release;
    }
    
    // 获取构建号和架构
    version.build = GetSysCtlString("kern.osversion");
    version.architecture = GetSysCtlString("hw.machine");
    
    return version;
}

MemoryInfo SystemInfo::GetMemoryInfo() {
    MemoryInfo info = {0, 0, 0, 0};
    
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    uint64_t total;
    size_t len = sizeof(total);
    if (sysctl(mib, 2, &total, &len, NULL, 0) == 0) {
        info.totalPhysical = total;
    }
    
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
    vm_statistics_data_t vm_stat;
    if (host_statistics(mach_host_self(), HOST_VM_INFO, 
                        reinterpret_cast<host_info_t>(&vm_stat), &count) == KERN_SUCCESS) {
        // 计算可用内存
        natural_t pagesize = getpagesize();
        info.availablePhysical = (vm_stat.free_count + vm_stat.inactive_count) * pagesize;
    }
    
    // macOS不提供虚拟内存统计
    info.totalVirtual = 0;
    info.availableVirtual = 0;
    
    return info;
}

CPUInfo SystemInfo::GetCPUInfo() {
    CPUInfo info;
    
    // 获取CPU厂商
    info.vendor = GetSysCtlString("machdep.cpu.vendor");
    
    // 获取CPU品牌
    info.brand = GetSysCtlString("machdep.cpu.brand_string");
    
    // 获取核心数
    info.cores = static_cast<uint32_t>(GetSysCtlUInt64("hw.physicalcpu"));
    
    // 获取线程数
    info.threads = static_cast<uint32_t>(GetSysCtlUInt64("hw.logicalcpu"));
    
    // 获取CPU频率 (MHz)
    uint64_t freq = GetSysCtlUInt64("hw.cpufrequency");
    info.clockSpeed = freq / 1000000000.0;
    
    return info;
}

std::chrono::system_clock::time_point SystemInfo::GetBootTime() {
    struct timeval bootTime;
    size_t len = sizeof(bootTime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    
    if (sysctl(mib, 2, &bootTime, &len, NULL, 0) != 0) {
        return std::chrono::system_clock::time_point();
    }
    
    return std::chrono::system_clock::from_time_t(bootTime.tv_sec) + 
           std::chrono::microseconds(bootTime.tv_usec);
}

std::chrono::milliseconds SystemInfo::GetUptime() {
    struct timeval bootTime;
    size_t len = sizeof(bootTime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    
    if (sysctl(mib, 2, &bootTime, &len, NULL, 0) != 0) {
        return std::chrono::milliseconds(0);
    }
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    long seconds = now.tv_sec - bootTime.tv_sec;
    long microseconds = now.tv_usec - bootTime.tv_usec;
    if (microseconds < 0) {
        seconds--;
        microseconds += 1000000;
    }
    
    return std::chrono::milliseconds(seconds * 1000 + microseconds / 1000);
}

SystemTimeInfo SystemInfo::GetSystemTimeInfo() {
    SystemTimeInfo info;
    info.currentTime = std::chrono::system_clock::now();
    info.bootTime = GetBootTime();
    return info;
}

std::string SystemInfo::GetUsername() {
    uid_t uid = geteuid();
    struct passwd* pw = getpwuid(uid);
    if (pw) {
        return pw->pw_name;
    }
    return "";
}

bool SystemInfo::IsAdmin() {
    return geteuid() == 0;
}

DiskSpaceInfo SystemInfo::GetDiskSpace(const std::string& path) {
    DiskSpaceInfo info = {0};
    
    struct statvfs vfs;
    if (statvfs(path.c_str(), &vfs) == 0) {
        info.totalSpace = static_cast<uint64_t>(vfs.f_blocks) * vfs.f_frsize;
        info.freeSpace = static_cast<uint64_t>(vfs.f_bfree) * vfs.f_frsize;
        info.availableSpace = static_cast<uint64_t>(vfs.f_bavail) * vfs.f_frsize;
    }
    
    return info;
}

std::vector<std::string> SystemInfo::GetMountPoints() {
    std::vector<std::string> mountPoints;
    
    struct statfs* mounts;
    int count = getmntinfo(&mounts, MNT_NOWAIT);
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            mountPoints.push_back(mounts[i].f_mntonname);
        }
    }
    
    return mountPoints;
}

} // namespace CorePlatform