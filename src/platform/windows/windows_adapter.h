#ifndef WINDOWS_ADAPTER_H
#define WINDOWS_ADAPTER_H

#include "ihelper/interfaces/iplatform_adapter.h"
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

class WindowsAdapter : public IPlatformAdapter {
public:
    WindowsAdapter();
    ~WindowsAdapter();
    
    nlohmann::json get_os_info() override;
    nlohmann::json get_hardware_info() override;
    nlohmann::json get_installed_software() override;
    nlohmann::json get_system_metrics() override;
    
private:
    IWbemLocator* pLoc_ = nullptr;
    IWbemServices* pSvc_ = nullptr;
    
    void init_wmi();
    nlohmann::json query_wmi(const std::wstring& query);
};

#endif // WINDOWS_ADAPTER_H