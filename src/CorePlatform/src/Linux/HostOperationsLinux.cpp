#include "CorePlatform/HostOperations.h"
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <systemd/sd-bus.h>
#include <sys/stat.h>
#include <pwd.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/mount.h>
#include <cctype>

namespace CorePlatform {

// 工具函数：获取错误信息
static std::error_code GetPosixError() {
    return std::error_code(errno, std::system_category());
}

// 工具函数：执行shell命令并获取输出
static std::string ExecCommand(const char* cmd) {
    std::string result;
    char buffer[128];
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    
    pclose(pipe);
    return result;
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
    std::ifstream cmdline("/proc/" + std::to_string(pid) + "/cmdline");
    if (cmdline) {
        std::string line;
        if (std::getline(cmdline, line)) {
            // cmdline是null分隔的，我们取第一个部分作为名称
            size_t pos = line.find('\0');
            if (pos != std::string::npos) {
                info.name = line.substr(0, pos);
            } else {
                info.name = line;
            }
            info.commandLine = line;
            // 替换null为空格
            std::replace(info.commandLine.begin(), info.commandLine.end(), '\0', ' ');
        }
    }
    
    // 获取内存使用
    std::ifstream status("/proc/" + std::to_string(pid) + "/status");
    if (status) {
        std::string line;
        while (std::getline(status, line)) {
            if (line.find("VmRSS:") == 0) {
                std::istringstream iss(line);
                std::string key;
                long value;
                iss >> key >> value;
                info.memoryUsage = value; // KB
            } else if (line.find("Uid:") == 0) {
                std::istringstream iss(line);
                std::string key;
                int uid;
                iss >> key >> uid;
                struct passwd* pwd = getpwuid(uid);
                if (pwd) {
                    info.owner = pwd->pw_name;
                }
            }
        }
    }
    
    // 获取启动时间
    std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
    if (stat) {
        std::string line;
        if (std::getline(stat, line)) {
            std::istringstream iss(line);
            std::string ignore;
            // 跳过前21个字段
            for (int i = 0; i < 21; i++) iss >> ignore;
            unsigned long starttime;
            iss >> starttime;
            // 转换为秒
            long uptime = 0;
            std::ifstream uptime_file("/proc/uptime");
            if (uptime_file) {
                uptime_file >> uptime;
            }
            info.startTime = time(nullptr) - uptime + starttime / sysconf(_SC_CLK_TCK);
        }
    }
    
    return info;
}

std::vector<ProcessInfo> ListProcesses() {
    std::vector<ProcessInfo> processes;
    
    DIR* dir = opendir("/proc");
    if (!dir) {
        throw HostException(GetPosixError(), "opendir failed");
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 只处理数字目录名
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            int pid = atoi(entry->d_name);
            try {
                processes.push_back(GetProcessInfo(pid));
            } catch (...) {
                // 忽略无法读取的进程
            }
        }
    }
    
    closedir(dir);
    return processes;
}

// 服务操作实现 (使用systemd)
static void ThrowDBusError(sd_bus_error* error) {
    if (error) {
        throw HostException(std::error_code(-1, std::generic_category()), 
                          std::string("DBus error: ") + error->message);
    }
    throw HostException(std::error_code(-1, std::generic_category()), "DBus error");
}

void InstallService(const std::string& name, 
                   const std::string& displayName,
                   const std::string& binPath) 
{
    // 创建服务文件
    std::string servicePath = "/etc/systemd/system/" + name + ".service";
    std::ofstream service(servicePath);
    if (!service) {
        throw HostException(GetPosixError(), "Failed to create service file");
    }
    
    service << "[Unit]\n"
            << "Description=" << displayName << "\n"
            << "\n"
            << "[Service]\n"
            << "ExecStart=" << binPath << "\n"
            << "Restart=always\n"
            << "\n"
            << "[Install]\n"
            << "WantedBy=multi-user.target\n";
    
    service.close();
    
    // 重新加载systemd
    if (system("systemctl daemon-reload") != 0) {
        throw HostException(GetPosixError(), "systemctl daemon-reload failed");
    }
}

void UninstallService(const std::string& name) {
    // 先停止服务
    try {
        StopService(name);
    } catch (...) {
        // 忽略停止错误
    }
    
    std::string servicePath = "/etc/systemd/system/" + name + ".service";
    if (unlink(servicePath.c_str()) != 0) {
        throw HostException(GetPosixError(), "unlink failed");
    }
    
    // 重新加载systemd
    if (system("systemctl daemon-reload") != 0) {
        throw HostException(GetPosixError(), "systemctl daemon-reload failed");
    }
}

void StartService(const std::string& name) {
    sd_bus* bus = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        throw HostException(std::error_code(-r, std::system_category()),
                          "Failed to connect to system bus");
    }
    
    r = sd_bus_call_method(bus,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          "StartUnit",
                          &error,
                          &reply,
                          "ss",
                          (name + ".service").c_str(),
                          "replace");
    
    if (r < 0) {
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        ThrowDBusError(&error);
    }
    
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
}

void StopService(const std::string& name) {
    sd_bus* bus = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        throw HostException(std::error_code(-r, std::system_category()),
                          "Failed to connect to system bus");
    }
    
    r = sd_bus_call_method(bus,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          "StopUnit",
                          &error,
                          &reply,
                          "ss",
                          (name + ".service").c_str(),
                          "replace");
    
    if (r < 0) {
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        ThrowDBusError(&error);
    }
    
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
}

ServiceInfo GetServiceInfo(const std::string& name) {
    ServiceInfo info;
    info.name = name;
    
    sd_bus* bus = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        throw HostException(std::error_code(-r, std::system_category()),
                          "Failed to connect to system bus");
    }
    
    r = sd_bus_call_method(bus,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          "GetUnit",
                          &error,
                          &reply,
                          "s",
                          (name + ".service").c_str());
    
    if (r < 0) {
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        ThrowDBusError(&error);
    }
    
    const char* unitPath = nullptr;
    r = sd_bus_message_read(reply, "o", &unitPath);
    if (r < 0) {
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        throw HostException(std::error_code(-r, std::system_category()),
                          "Failed to parse GetUnit response");
    }
    
    sd_bus_message_unref(reply);
    reply = nullptr;
    
    r = sd_bus_call_method(bus,
                          "org.freedesktop.systemd1",
                          unitPath,
                          "org.freedesktop.DBus.Properties",
                          "GetAll",
                          &error,
                          &reply,
                          "s",
                          "org.freedesktop.systemd1.Unit");
    
    if (r < 0) {
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        ThrowDBusError(&error);
    }
    
    r = sd_bus_message_enter_container(reply, 'a', "{sv}");
    if (r < 0) {
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        throw HostException(std::error_code(-r, std::system_category()),
                          "Failed to enter container");
    }
    
    while ((r = sd_bus_message_enter_container(reply, 'e', "sv")) > 0) {
        const char* property = nullptr;
        r = sd_bus_message_read(reply, "s", &property);
        if (r < 0) break;
        
        const char* type = nullptr;
        sd_bus_message* variant = nullptr;
        r = sd_bus_message_peek_type(reply, &type, &variant);
        if (r < 0) break;
        
        if (strcmp(property, "Description") == 0) {
            const char* description = nullptr;
            sd_bus_message_read_variant(variant, "s", &description);
            info.displayName = description;
        } else if (strcmp(property, "ActiveState") == 0) {
            const char* state = nullptr;
            sd_bus_message_read_variant(variant, "s", &state);
            if (strcmp(state, "active") == 0) {
                info.status = ServiceStatus::Running;
            } else if (strcmp(state, "activating") == 0) {
                info.status = ServiceStatus::StartPending;
            } else if (strcmp(state, "deactivating") == 0) {
                info.status = ServiceStatus::StopPending;
            } else {
                info.status = ServiceStatus::Stopped;
            }
        }
        
        sd_bus_message_skip(reply, "v");
        sd_bus_message_exit_container(reply);
    }
    
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return info;
}

std::vector<ServiceInfo> ListServices() {
    std::vector<ServiceInfo> services;
    
    sd_bus* bus = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        throw HostException(std::error_code(-r, std::system_category()),
                          "Failed to connect to system bus");
    }
    
    r = sd_bus_call_method(bus,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          "ListUnits",
                          &error,
                          &reply,
                          "");
    
    if (r < 0) {
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        ThrowDBusError(&error);
    }
    
    r = sd_bus_message_enter_container(reply, 'a', "(ssssssouso)");
    if (r < 0) {
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        throw HostException(std::error_code(-r, std::system_category()),
                          "Failed to enter container");
    }
    
    while ((r = sd_bus_message_read(reply, "(ssssssouso)", 
                                   nullptr, nullptr, nullptr, nullptr, 
                                   nullptr, nullptr, nullptr, 
                                   nullptr, nullptr, nullptr)) > 0) 
    {
        const char* unitName = nullptr;
        const char* description = nullptr;
        const char* loadState = nullptr;
        const char* activeState = nullptr;
        
        r = sd_bus_message_read(reply, "(ssssssouso)", 
                               &unitName, &description, &loadState, &activeState,
                               nullptr, nullptr, nullptr, 
                               nullptr, nullptr, nullptr);
        
        if (r < 0) break;
        
        // 只处理服务
        if (strstr(unitName, ".service") != nullptr) {
            ServiceInfo info;
            info.name = unitName;
            info.displayName = description ? description : "";
            
            if (strcmp(activeState, "active") == 0) {
                info.status = ServiceStatus::Running;
            } else if (strcmp(activeState, "activating") == 0) {
                info.status = ServiceStatus::StartPending;
            } else if (strcmp(activeState, "deactivating") == 0) {
                info.status = ServiceStatus::StopPending;
            } else {
                info.status = ServiceStatus::Stopped;
            }
            
            services.push_back(info);
        }
    }
    
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return services;
}

// 驱动操作实现 (内核模块)
void InstallDriver(const std::string& name, const std::string& filePath) {
    // Linux驱动是内核模块(.ko文件)
    // 安装通常意味着复制到/lib/modules目录
    std::string destPath = "/lib/modules/$(uname -r)/kernel/drivers/" + name + ".ko";
    
    // 复制文件
    std::ifstream src(filePath, std::ios::binary);
    if (!src) {
        throw HostException(GetPosixError(), "Failed to open source file");
    }
    
    std::ofstream dest(destPath, std::ios::binary);
    if (!dest) {
        throw HostException(GetPosixError(), "Failed to open destination file");
    }
    
    dest << src.rdbuf();
    
    // 更新模块依赖
    if (system("depmod -a") != 0) {
        throw HostException(GetPosixError(), "depmod failed");
    }
}

void UninstallDriver(const std::string& name) {
    std::string destPath = "/lib/modules/$(uname -r)/kernel/drivers/" + name + ".ko";
    if (unlink(destPath.c_str()) != 0) {
        throw HostException(GetPosixError(), "unlink failed");
    }
    
    // 更新模块依赖
    if (system("depmod -a") != 0) {
        throw HostException(GetPosixError(), "depmod failed");
    }
}

void LoadDriver(const std::string& name) {
    std::string cmd = "/sbin/modprobe " + name;
    if (system(cmd.c_str()) != 0) {
        throw HostException(GetPosixError(), "modprobe failed");
    }
}

void UnloadDriver(const std::string& name) {
    std::string cmd = "/sbin/rmmod " + name;
    if (system(cmd.c_str()) != 0) {
        throw HostException(GetPosixError(), "rmmod failed");
    }
}

DriverInfo GetDriverInfo(const std::string& name) {
    DriverInfo info;
    info.name = name;
    
    // 检查模块是否加载
    std::ifstream modules("/proc/modules");
    if (!modules) {
        throw HostException(GetPosixError(), "Failed to open /proc/modules");
    }
    
    std::string line;
    while (std::getline(modules, line)) {
        if (line.find(name) == 0) {
            info.status = DriverStatus::Running;
            break;
        }
    }
    
    if (info.status != DriverStatus::Running) {
        info.status = DriverStatus::Stopped;
    }
    
    // 获取模块路径
    std::string modinfo = ExecCommand(("/sbin/modinfo -F filename " + name).c_str());
    if (!modinfo.empty()) {
        // 移除换行符
        modinfo.erase(std::remove(modinfo.begin(), modinfo.end(), '\n'), modinfo.end());
        info.filePath = modinfo;
    }
    
    return info;
}

} // namespace CorePlatform