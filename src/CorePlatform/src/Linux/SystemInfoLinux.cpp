#include "CorePlatform/SystemInfo.h"
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <pwd.h>
#include <cctype>
#include <dirent.h>
#include <regex>

namespace CorePlatform {

static std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

SystemVersion SystemInfo::GetOSVersion() {
    SystemVersion version;
    
    struct utsname uts;
    if (uname(&uts) == 0) {
        version.name = uts.sysname;
        version.version = uts.release;
        version.build = uts.version;
        version.architecture = uts.machine;
    }
    
    // 尝试从/etc/os-release获取更友好的名称
    std::ifstream osRelease("/etc/os-release");
    if (osRelease.is_open()) {
        std::string line;
        while (std::getline(osRelease, line)) {
            if (line.find("PRETTY_NAME=") == 0) {
                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    // 移除引号
                    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                        value = value.substr(1, value.size() - 2);
                    }
                    version.name = value;
                }
                break;
            }
        }
    }
    
    return version;
}

MemoryInfo SystemInfo::GetMemoryInfo() {
    MemoryInfo info = {0, 0, 0, 0};
    
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.totalPhysical = si.totalram * si.mem_unit;
        info.availablePhysical = si.freeram * si.mem_unit;
        info.totalVirtual = info.totalPhysical + (si.totalswap * si.mem_unit);
        info.availableVirtual = info.availablePhysical + (si.freeswap * si.mem_unit);
    }
    
    return info;
}

CPUInfo SystemInfo::GetCPUInfo() {
    CPUInfo info;
    
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        bool firstProcessor = true;
        
        while (std::getline(cpuinfo, line)) {
            // 获取厂商信息
            if (line.find("vendor_id") == 0) {
                size_t pos = line.find(':');
                if (pos != std::string::npos && info.vendor.empty()) {
                    info.vendor = Trim(line.substr(pos + 1));
                }
            }
            // 获取品牌信息
            else if (line.find("model name") == 0) {
                size_t pos = line.find(':');
                if (pos != std::string::npos && info.brand.empty()) {
                    info.brand = Trim(line.substr(pos + 1));
                }
            }
            // 获取物理核心数
            else if (line.find("cpu cores") == 0) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    info.cores = std::stoul(Trim(line.substr(pos + 1)));
                }
            }
            // 获取CPU频率
            else if (line.find("cpu MHz") == 0) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    info.clockSpeed = std::stod(Trim(line.substr(pos + 1))) / 1000.0;
                }
            }
            // 计数处理器
            else if (line.find("processor") == 0) {
                info.threads++;
            }
        }
        
        // 如果没有获取到物理核心数，使用线程数
        if (info.cores == 0) {
            info.cores = info.threads;
        }
    }
    
    return info;
}

std::chrono::system_clock::time_point SystemInfo::GetBootTime() {
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        return std::chrono::system_clock::time_point();
    }
    
    auto now = std::chrono::system_clock::now();
    return now - std::chrono::seconds(si.uptime);
}

std::chrono::milliseconds SystemInfo::GetUptime() {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return std::chrono::seconds(si.uptime);
    }
    return std::chrono::milliseconds(0);
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
    
    std::ifstream mounts("/proc/mounts");
    if (mounts.is_open()) {
        std::string line;
        while (std::getline(mounts, line)) {
            std::istringstream iss(line);
            std::string device, mountPoint, type;
            iss >> device >> mountPoint >> type;
            
            // 过滤特殊文件系统
            if (mountPoint.find("/dev") == 0 || 
                mountPoint.find("/proc") == 0 || 
                mountPoint.find("/sys") == 0 ||
                mountPoint.find("/run") == 0) {
                continue;
            }
            
            mountPoints.push_back(mountPoint);
        }
    }
    
    return mountPoints;
}

} // namespace CorePlatform