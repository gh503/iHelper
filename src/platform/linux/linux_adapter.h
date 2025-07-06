#ifndef LINUX_ADAPTER_H
#define LINUX_ADAPTER_H

#include "ihelper/interfaces/iplatform_adapter.h"
#include <fstream>
#include <sstream>

class LinuxAdapter : public IPlatformAdapter {
public:
    nlohmann::json get_os_info() override;
    nlohmann::json get_hardware_info() override;
    nlohmann::json get_installed_software() override;
    nlohmann::json get_system_metrics() override;
    
private:
    std::string read_file(const std::string& path);
    std::string exec_command(const char* cmd);
    std::vector<std::string> split(const std::string& str, char delimiter);
};

#endif // LINUX_ADAPTER_H