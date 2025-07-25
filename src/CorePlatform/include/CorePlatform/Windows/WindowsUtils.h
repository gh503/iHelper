#pragma once
#include <Windows.h>
#include <memory>
#include <string>
#include <vector>
#include "CorePlatform/Export.h"

namespace CorePlatform {

class CORE_PLATFORM_API WindowsUtils {
public:
    static std::string WideToUTF8(const std::wstring& wstr);
    static std::wstring UTF8ToWide(const std::string& str);
    
    // 获取错误消息
    static std::string GetLastErrorString(DWORD errorCode = 0);
    
    // 安全句柄关闭器
    struct HandleCloser {
        void operator()(void* handle) const {
            if (handle && handle != INVALID_HANDLE_VALUE) {
                CloseHandle(handle);
            }
        }
    };
    
    using ScopedHandle = std::unique_ptr<void, HandleCloser>;
    
    // 注册表句柄关闭器
    struct RegKeyCloser {
        void operator()(HKEY hKey) const {
            if (hKey) {
                RegCloseKey(hKey);
            }
        }
    };
    
    using ScopedRegKey = std::unique_ptr<std::remove_pointer<HKEY>::type, RegKeyCloser>;
};

} // namespace CorePlatform