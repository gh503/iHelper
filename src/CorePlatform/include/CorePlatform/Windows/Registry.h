#pragma once
#include <string>
#include <vector>
#include <system_error>
#include "CorePlatform/Export.h"

namespace CorePlatform {

class CORE_PLATFORM_API Registry {
public:
    // 注册表根键枚举
    enum class RootKey {
        ClassesRoot,   // HKEY_CLASSES_ROOT
        CurrentUser,   // HKEY_CURRENT_USER
        LocalMachine,  // HKEY_LOCAL_MACHINE
        Users,         // HKEY_USERS
        CurrentConfig, // HKEY_CURRENT_CONFIG
        PerformanceData // HKEY_PERFORMANCE_DATA
    };

    // 注册表视图选项 (解决重定向问题)
    enum class RegistryView {
        Default,    // 系统默认视图
        Force32,    // 强制32位视图 (KEY_WOW64_32KEY)
        Force64     // 强制64位视图 (KEY_WOW64_64KEY)
    };

    // 注册表值类型
    enum class ValueType {
        None,          // 无值
        String,        // 字符串值
        ExpandString,  // 可扩展字符串
        Binary,        // 二进制数据
        DWord,         // 32位整数
        DWordBigEndian,// 大端32位整数
        Link,          // 符号链接
        MultiString,   // 多字符串
        QWord          // 64位整数
    };

    // 注册表值信息
    struct ValueInfo {
        std::string name;
        ValueType type = ValueType::None;
        std::vector<uint8_t> data;
    };

    // 检查键是否存在
    static bool KeyExists(RootKey root, const std::string& subkey, 
                         RegistryView view = RegistryView::Default);
    
    // 创建注册表键
    static bool CreateKey(RootKey root, const std::string& subkey, 
                         RegistryView view = RegistryView::Default);
    
    // 删除注册表键（递归）
    static bool DeleteKey(RootKey root, const std::string& subkey, 
                         RegistryView view = RegistryView::Default);
    
    // 删除值
    static bool DeleteValue(RootKey root, const std::string& subkey, 
                           const std::string& value, 
                           RegistryView view = RegistryView::Default);
    
    // 读取字符串值
    static bool ReadString(RootKey root, const std::string& subkey, 
                          const std::string& value, std::string& result,
                          RegistryView view = RegistryView::Default);
    
    // 写入字符串值
    static bool WriteString(RootKey root, const std::string& subkey,
                           const std::string& value, const std::string& data,
                           RegistryView view = RegistryView::Default);
    
    // 读取DWORD值
    static bool ReadDWord(RootKey root, const std::string& subkey,
                         const std::string& value, uint32_t& result,
                         RegistryView view = RegistryView::Default);
    
    // 写入DWORD值
    static bool WriteDWord(RootKey root, const std::string& subkey,
                          const std::string& value, uint32_t data,
                          RegistryView view = RegistryView::Default);
    
    // 读取QWORD值
    static bool ReadQWord(RootKey root, const std::string& subkey,
                         const std::string& value, uint64_t& result,
                         RegistryView view = RegistryView::Default);
    
    // 写入QWORD值
    static bool WriteQWord(RootKey root, const std::string& subkey,
                          const std::string& value, uint64_t data,
                          RegistryView view = RegistryView::Default);
    
    // 读取二进制值
    static bool ReadBinary(RootKey root, const std::string& subkey,
                          const std::string& value, std::vector<uint8_t>& result,
                          RegistryView view = RegistryView::Default);
    
    // 写入二进制值
    static bool WriteBinary(RootKey root, const std::string& subkey,
                           const std::string& value, const std::vector<uint8_t>& data,
                           RegistryView view = RegistryView::Default);
    
    // 枚举子键
    static bool EnumSubKeys(RootKey root, const std::string& subkey,
                           std::vector<std::string>& result,
                           RegistryView view = RegistryView::Default);
    
    // 枚举值
    static bool EnumValues(RootKey root, const std::string& subkey,
                          std::vector<ValueInfo>& result,
                          RegistryView view = RegistryView::Default);
};

} // namespace CorePlatform