#include "CorePlatform/Windows/UAC.h"
#include "CorePlatform/Windows/WindowsUtils.h"
#include <shellapi.h>
#include <sddl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cctype>
#include <locale>
#include <codecvt>

namespace CorePlatform {

namespace {

// 从进程令牌获取信息
bool get_token_information(HANDLE token, TOKEN_INFORMATION_CLASS info_class, 
                          std::vector<BYTE>& buffer) {
    DWORD size = 0;
    if (!GetTokenInformation(token, info_class, nullptr, 0, &size) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return false;
    }
    
    buffer.resize(size);
    if (!GetTokenInformation(token, info_class, buffer.data(), size, &size)) {
        return false;
    }
    
    return true;
}

// 检查是否为管理员组
bool is_admin_group(PSID sid) {
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID admin_group = nullptr;
    
    if (!AllocateAndInitializeSid(
            &nt_authority, 
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &admin_group)) {
        return false;
    }
    
    bool result = IsValidSid(sid) && IsValidSid(admin_group) &&
                  EqualSid(sid, admin_group);
    
    if (admin_group) {
        FreeSid(admin_group);
    }
    
    return result;
}

} // 匿名命名空间

bool is_running_as_admin() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    
    std::vector<BYTE> buffer;
    if (!get_token_information(token, TokenGroups, buffer)) {
        CloseHandle(token);
        return false;
    }
    
    PTOKEN_GROUPS groups = reinterpret_cast<PTOKEN_GROUPS>(buffer.data());
    for (DWORD i = 0; i < groups->GroupCount; i++) {
        if ((groups->Groups[i].Attributes & SE_GROUP_ENABLED) &&
            (groups->Groups[i].Attributes & SE_GROUP_USE_FOR_DENY_ONLY) == 0 &&
            is_admin_group(groups->Groups[i].Sid)) {
            CloseHandle(token);
            return true;
        }
    }
    
    CloseHandle(token);
    return false;
}

ExecutionLevel get_requested_execution_level() {
    // 实际实现需要解析应用程序清单
    // 这里简化为检查是否在管理员权限下运行
    return is_running_as_admin() ? 
        ExecutionLevel::RequireAdministrator : 
        ExecutionLevel::AsInvoker;
}

ExecutionLevel get_actual_execution_level() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return ExecutionLevel::Unknown;
    }
    
    DWORD elevation_type = 0;
    DWORD size = sizeof(elevation_type);
    if (!GetTokenInformation(token, TokenElevationType, &elevation_type, size, &size)) {
        CloseHandle(token);
        return ExecutionLevel::Unknown;
    }
    
    CloseHandle(token);
    
    switch (elevation_type) {
        case TokenElevationTypeDefault: // 用户没有分离令牌
            return ExecutionLevel::AsInvoker;
        case TokenElevationTypeFull:    // 完全管理员令牌
            return ExecutionLevel::RequireAdministrator;
        case TokenElevationTypeLimited: // 有限令牌
            return ExecutionLevel::HighestAvailable;
        default:
            return ExecutionLevel::Unknown;
    }
}

std::pair<HANDLE, DWORD> elevate_privileges(
    const std::string& parameters,
    bool wait,
    bool show_error
) {
    wchar_t path[MAX_PATH] = {0};
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) {
        return {nullptr, 0};
    }
    
    // 将参数转换为宽字符串
    std::wstring wide_parameters = WindowsUtils::UTF8ToWide(parameters);
    
    SHELLEXECUTEINFOW shell_info = {};
    shell_info.cbSize = sizeof(shell_info);
    shell_info.lpVerb = L"runas";
    shell_info.lpFile = path;
    shell_info.lpParameters = wide_parameters.c_str();
    shell_info.nShow = SW_NORMAL;
    shell_info.fMask = SEE_MASK_NOCLOSEPROCESS;
    
    if (!ShellExecuteExW(&shell_info)) {
        DWORD error = GetLastError();
        
        if (show_error && error != ERROR_CANCELLED) {
            std::string message = "无法提升权限。错误代码: " + std::to_string(error);
            std::wstring wide_message = WindowsUtils::UTF8ToWide(message);
            MessageBoxW(nullptr, wide_message.c_str(), L"权限提升失败", MB_OK | MB_ICONERROR);
        }
        
        return {nullptr, 0};
    }
    
    DWORD pid = GetProcessId(shell_info.hProcess);
    
    if (wait) {
        WaitForSingleObject(shell_info.hProcess, INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(shell_info.hProcess, &exit_code);
        CloseHandle(shell_info.hProcess);
        return {nullptr, exit_code};
    }
    
    return {shell_info.hProcess, pid};
}

bool ensure_admin_privileges(bool restart, const std::string& parameters) {
    if (is_running_as_admin()) {
        return true;
    }
    
    auto [process, pid] = elevate_privileges(parameters, false);
    if (process) {
        if (restart) {
            // 退出当前进程，让提升后的进程运行
            ExitProcess(0);
        }
        return true;
    }
    
    return false;
}

bool check_uac_settings(ExecutionLevel required_level, bool auto_elevate) {
    ExecutionLevel actual_level = get_actual_execution_level();
    
    if (actual_level >= required_level) {
        return true;
    }
    
    if (auto_elevate) {
        return ensure_admin_privileges();
    }
    
    // 显示权限不足警告
    std::string message = "此操作需要管理员权限。\n";
    message += "当前权限级别: " + execution_level_to_string(actual_level) + "\n";
    message += "需要权限级别: " + execution_level_to_string(required_level);
    
    std::wstring wide_message = WindowsUtils::UTF8ToWide(message);
    std::wstring wide_title = WindowsUtils::UTF8ToWide("权限不足");
    
    MessageBoxW(nullptr, wide_message.c_str(), wide_title.c_str(), MB_OK | MB_ICONWARNING);
    return false;
}

std::string execution_level_to_string(ExecutionLevel level) {
    switch (level) {
        case ExecutionLevel::AsInvoker:
            return "标准用户权限 (asInvoker)";
        case ExecutionLevel::HighestAvailable:
            return "最高可用权限 (highestAvailable)";
        case ExecutionLevel::RequireAdministrator:
            return "管理员权限 (requireAdministrator)";
        default:
            return "未知权限";
    }
}

} // namespace CorePlatform