#include <iostream>
#include "version_iHelper.h" // 自动生成的头文件
#include "CorePlatform/Windows/UAC.h"

void perform_admin_operation() {
    // 尝试写入需要管理员权限的位置
    HKEY hKey;
    LSTATUS result = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\MyElevatedApp",
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE | KEY_READ,
        NULL,
        &hKey,
        NULL
    );

    if (result == ERROR_SUCCESS) {
        std::cout << "成功写入注册表，程序以管理员权限运行！" << std::endl;
        
        // 写入示例值
        const wchar_t* value = L"Admin Access Verified";
        RegSetValueExW(hKey, L"Status", 0, REG_SZ, 
                      (const BYTE*)value, (wcslen(value) + 1) * sizeof(wchar_t));
        
        RegCloseKey(hKey);
    } else {
        std::cerr << "无法写入注册表，错误代码: " << result << std::endl;
    }
}

int main(const int argc, const char* argv[])
{
    // 设置控制台为 UTF-8 编码
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "hello iHelper" << std::endl;
    std::cout << "====================================\n";
    std::cout << " " << iHelper_NAME << " v" << iHelper_VERSION_STRING << "\n";
    std::cout << " Build: " << iHelper_BUILD_DATE 
              << " (" << iHelper_GIT_COMMIT_HASH << ")\n";
    std::cout << " " << iHelper_COPYRIGHT << "\n";
    std::cout << "====================================\n\n";

    // 检查并处理 UAC 设置
    if (!CorePlatform::check_uac_settings()) {
        std::cerr << "权限不足，操作已取消。" << std::endl;
        return 1;
    }
    
    // 主程序逻辑
    std::cout << "程序以管理员权限运行！" << std::endl;

    // 获取并显示当前权限级别
    auto level = CorePlatform::get_actual_execution_level();
    std::cout << "当前权限级别: " 
              << CorePlatform::execution_level_to_string(level) 
              << std::endl;
    
    // 执行需要管理员权限的操作
    perform_admin_operation();
    
    // 示例：重启程序并传递参数
    if (argc > 1 && std::string(argv[1]) == "--restart") {
        std::cout << "程序已成功重启！" << std::endl;
    }

    return 0;
}