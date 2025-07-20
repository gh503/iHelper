#include "CorePlatform/Windows/WindowsUtils.h"
#include <stringapiset.h>
#include <sstream>

namespace CorePlatform {

std::string WindowsUtils::WideToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    
    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), (int)wstr.size(), 
        nullptr, 0, nullptr, nullptr
    );
    
    if (size_needed <= 0) return "";
    
    std::string str(size_needed, 0);
    WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), (int)wstr.size(),
        str.data(), size_needed, nullptr, nullptr
    );
    
    return str;
}

std::wstring WindowsUtils::UTF8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    
    int size_needed = MultiByteToWideChar(
        CP_UTF8, 0, str.data(), (int)str.size(), 
        nullptr, 0
    );
    
    if (size_needed <= 0) return L"";
    
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(
        CP_UTF8, 0, str.data(), (int)str.size(),
        wstr.data(), size_needed
    );
    
    return wstr;
}

std::string WindowsUtils::GetLastErrorString(DWORD errorCode) {
    if (errorCode == 0) {
        errorCode = GetLastError();
    }
    
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        nullptr
    );
    
    if (size == 0) {
        return "Unknown error";
    }
    
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    
    // 移除Windows错误消息中常见的换行符
    if (!message.empty() && message.back() == '\n') {
        message.pop_back();
    }
    if (!message.empty() && message.back() == '\r') {
        message.pop_back();
    }
    
    return message;
}

} // namespace CorePlatform