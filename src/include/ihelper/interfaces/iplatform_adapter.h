#ifndef IHELPER_IPLATFORM_ADAPTER_H
#define IHELPER_IPLATFORM_ADAPTER_H

#include <nlohmann/json.hpp>

class IPlatformAdapter {
public:
    virtual ~IPlatformAdapter() = default;
    
    virtual nlohmann::json get_os_info() = 0;
    virtual nlohmann::json get_hardware_info() = 0;
    virtual nlohmann::json get_installed_software() = 0;
    virtual nlohmann::json get_system_metrics() = 0;
};

#endif // IHELPER_IPLATFORM_ADAPTER_H