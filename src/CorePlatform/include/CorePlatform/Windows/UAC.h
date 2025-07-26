#pragma once
#include <string>
#include <vector>
#include <Windows.h>
#include "CorePlatform/Export.h"

namespace CorePlatform {

// UAC 执行级别
enum class ExecutionLevel {
    Unknown,
    AsInvoker,
    HighestAvailable,
    RequireAdministrator
};

/**
 * @brief 检查当前进程是否以管理员权限运行
 */
bool CORE_PLATFORM_API is_running_as_admin();

/**
 * @brief 获取当前进程的请求执行级别
 */
ExecutionLevel CORE_PLATFORM_API get_requested_execution_level();

/**
 * @brief 获取当前进程的实际执行级别
 */
ExecutionLevel CORE_PLATFORM_API get_actual_execution_level();

/**
 * @brief 尝试提升当前进程权限
 * @param parameters 提权后进程的命令行参数
 * @param wait 是否等待提升的进程完成
 * @param show_error 是否在失败时显示错误消息
 * @return 进程句柄和进程ID（如果wait=false），或提升结果
 */
std::pair<HANDLE, DWORD> CORE_PLATFORM_API elevate_privileges(
    const std::string& parameters = "",
    bool wait = true,
    bool show_error = true
);

/**
 * @brief 确保程序以管理员权限运行
 * @param restart 是否在提权后重启当前程序
 * @param parameters 重启时的命令行参数
 * @return 是否以管理员权限运行
 */
bool CORE_PLATFORM_API ensure_admin_privileges(
    bool restart = true,
    const std::string& parameters = ""
);

/**
 * @brief 检查并处理 UAC 设置
 * @param required_level 需要的执行级别
 * @param auto_elevate 是否自动尝试提权
 * @return 是否满足权限要求
 */
bool CORE_PLATFORM_API check_uac_settings(
    ExecutionLevel required_level = ExecutionLevel::RequireAdministrator,
    bool auto_elevate = true
);

/**
 * @brief 获取 UAC 级别描述字符串
 */
std::string CORE_PLATFORM_API execution_level_to_string(ExecutionLevel level);

} // namespace CorePlatform