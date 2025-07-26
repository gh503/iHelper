#include "CorePlatform/HostOperations.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <libproc.h>
#include <pwd.h>
#include <signal.h>
#include <launch.h>
#include <mach/mach.h>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace CorePlatform {

// 工具函数：获取错误信息
static std::error_code GetPosixError() {
    return std::error_code(errno, std::system_category());
}

// 进程操作实现
void StartProcess(const std::string& path, const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == -1) {
        throw HostException(GetPosixError(), "fork failed");
    }
    
    if (pid == 0) { // 子进程
        // 准备参数
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(path.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        // 执行程序
        execvp(path.c_str(), argv.data());
        _exit(EXIT_FAILURE); // 如果execvp失败
    }
}

void TerminateProcess(int pid) {
    if (kill(pid, SIGTERM) != 0) {
        throw HostException(GetPosixError(), "kill failed");
    }
}

ProcessInfo GetProcessInfo(int pid) {
    ProcessInfo info;
    info.pid = pid;
    
    // 获取进程名
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof(path)) > 0) {
        info.name = path;
    }
    
    // 获取内存使用
    struct proc_taskinfo pti;
    int size = sizeof(pti);
    if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &pti, size) == size) {
        info.memoryUsage = pti.pti_resident_size / 1024;
    }
    
    // 获取启动时间
    struct proc_bsdinfo pbi;
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &pbi, sizeof(pbi)) > 0) {
        info.startTime = pbi.pbi_start_tvsec;
    }
    
    // 获取命令行
    int mib[3] = {CTL_KERN, KERN_PROCARGS2, pid};
    size_t argmax = 4096;
    std::vector<char> buffer(argmax);
    
    if (sysctl(mib, 3, buffer.data(), &argmax, nullptr, 0) == 0) {
        char* cp = buffer.data();
        int argc = *reinterpret_cast<int*>(cp);
        cp += sizeof(int);
        
        // 跳过可执行路径
        while (*cp != '\0') cp++;
        cp++;
        
        // 收集所有参数
        std::ostringstream oss;
        for (int i = 0; i < argc && cp < buffer.data() + argmax; i++) {
            if (i > 0) oss << " ";
            oss << cp;
            while (*cp != '\0') cp++;
            cp++;
        }
        info.commandLine = oss.str();
    }
    
    // 获取所有者
    struct passwd* pwd = getpwuid(getuid());
    if (pwd) {
        info.owner = pwd->pw_name;
    }
    
    return info;
}

std::vector<ProcessInfo> ListProcesses() {
    std::vector<ProcessInfo> processes;
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t len;
    
    if (sysctl(mib, 4, nullptr, &len, nullptr, 0) == -1) {
        throw HostException(GetPosixError(), "sysctl failed");
    }
    
    size_t count = len / sizeof(struct kinfo_proc);
    std::vector<struct kinfo_proc> buffer(count);
    
    if (sysctl(mib, 4, buffer.data(), &len, nullptr, 0) == -1) {
        throw HostException(GetPosixError(), "sysctl failed");
    }
    
    for (size_t i = 0; i < count; i++) {
        ProcessInfo info;
        info.pid = buffer[i].kp_proc.p_pid;
        if (buffer[i].kp_proc.p_comm) {
            info.name = buffer[i].kp_proc.p_comm;
        }
        processes.push_back(info);
    }
    
    return processes;
}

// 服务操作实现 (使用launchd)
void InstallService(const std::string& name, 
                   const std::string& displayName,
                   const std::string& binPath) 
{
    // 创建plist文件
    std::string plistPath = "/Library/LaunchDaemons/" + name + ".plist";
    std::ofstream plist(plistPath);
    if (!plist) {
        throw HostException(GetPosixError(), "Failed to create plist file");
    }
    
    plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
          << "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
          << "<plist version=\"1.0\">\n"
          << "<dict>\n"
          << "  <key>Label</key>\n"
          << "  <string>" << name << "</string>\n"
          << "  <key>Program</key>\n"
          << "  <string>" << binPath << "</string>\n"
          << "  <key>RunAtLoad</key>\n"
          << "  <true/>\n"
          << "  <key>KeepAlive</key>\n"
          << "  <true/>\n"
          << "</dict>\n"
          << "</plist>\n";
    
    plist.close();
    
    // 设置权限
    if (chown(plistPath.c_str(), 0, 0) != 0) {
        throw HostException(GetPosixError(), "chown failed");
    }
    
    if (chmod(plistPath.c_str(), 0644) != 0) {
        throw HostException(GetPosixError(), "chmod failed");
    }
}

void UninstallService(const std::string& name) {
    // 先停止服务
    try {
        StopService(name);
    } catch (...) {
        // 忽略停止错误
    }
    
    std::string plistPath = "/Library/LaunchDaemons/" + name + ".plist";
    if (unlink(plistPath.c_str()) != 0) {
        throw HostException(GetPosixError(), "unlink failed");
    }
}

void StartService(const std::string& name) {
    std::string cmd = "/bin/launchctl load -w /Library/LaunchDaemons/" + name + ".plist";
    if (system(cmd.c_str()) != 0) {
        throw HostException(GetPosixError(), "launchctl load failed");
    }
}

void StopService(const std::string& name) {
    std::string cmd = "/bin/launchctl unload /Library/LaunchDaemons/" + name + ".plist";
    if (system(cmd.c_str()) != 0) {
        throw HostException(GetPosixError(), "launchctl unload failed");
    }
}

ServiceInfo GetServiceInfo(const std::string& name) {
    ServiceInfo info;
    info.name = name;
    
    // 使用launchctl list获取服务状态
    std::string cmd = "/bin/launchctl list | grep " + name;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw HostException(GetPosixError(), "popen failed");
    }
    
    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        // 检查状态码
        if (line.find("\t0") != std::string::npos) {
            info.status = ServiceStatus::Running;
        } else {
            info.status = ServiceStatus::Stopped;
        }
    }
    
    pclose(pipe);
    return info;
}

std::vector<ServiceInfo> ListServices() {
    // macOS没有直接获取所有服务列表的方法
    // 这里仅返回空列表作为示例
    return {};
}

// 驱动操作实现 (macOS驱动管理需要内核扩展)
void InstallDriver(const std::string& name, const std::string& filePath) {
    // macOS驱动安装需要内核扩展(kext)
    // 这里仅作为示例
    throw HostException(std::make_error_code(std::errc::not_supported), 
                      "Driver installation not supported on macOS");
}

void UninstallDriver(const std::string& name) {
    throw HostException(std::make_error_code(std::errc::not_supported), 
                      "Driver uninstallation not supported on macOS");
}

void LoadDriver(const std::string& name) {
    throw HostException(std::make_error_code(std::errc::not_supported), 
                      "Driver loading not supported on macOS");
}

void UnloadDriver(const std::string& name) {
    throw HostException(std::make_error_code(std::errc::not_supported), 
                      "Driver unloading not supported on macOS");
}

DriverInfo GetDriverInfo(const std::string& name) {
    throw HostException(std::make_error_code(std::errc::not_supported), 
                      "Driver info not supported on macOS");
}

} // namespace CorePlatform