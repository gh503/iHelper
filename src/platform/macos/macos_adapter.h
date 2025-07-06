#ifndef MACOS_ADAPTER_H
#define MACOS_ADAPTER_H

#include "ihelper/interfaces/iplatform_adapter.h"
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>

class MacOSAdapter : public IPlatformAdapter {
public:
    nlohmann::json get_os_info() override;
    nlohmann::json get_hardware_info() override;
    nlohmann::json get_installed_software() override;
    nlohmann::json get_system_metrics() override;
    
private:
    std::string exec_command(const char* cmd);
    uint64_t get_sysctl_value(const char* name);
};

#endif // MACOS_ADAPTER_H