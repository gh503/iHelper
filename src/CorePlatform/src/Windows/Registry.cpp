#include "CorePlatform/Windows/Registry.h"
#include "CorePlatform/Windows/WindowsUtils.h"
#include <Windows.h>
#include <winreg.h>
#include <vector>
#include <memory>
#include <system_error>

namespace CorePlatform {

namespace {

// 将RootKey转换为Windows HKEY
HKEY RootKeyToHKEY(Registry::RootKey root) {
    switch (root) {
        case Registry::RootKey::ClassesRoot:    return HKEY_CLASSES_ROOT;
        case Registry::RootKey::CurrentUser:    return HKEY_CURRENT_USER;
        case Registry::RootKey::LocalMachine:   return HKEY_LOCAL_MACHINE;
        case Registry::RootKey::Users:          return HKEY_USERS;
        case Registry::RootKey::CurrentConfig:  return HKEY_CURRENT_CONFIG;
        case Registry::RootKey::PerformanceData:return HKEY_PERFORMANCE_DATA;
        default: return nullptr;
    }
}

// 转换Windows注册表类型到ValueType
Registry::ValueType WinRegTypeToValueType(DWORD type) {
    switch (type) {
        case REG_SZ:        return Registry::ValueType::String;
        case REG_EXPAND_SZ:  return Registry::ValueType::ExpandString;
        case REG_BINARY:    return Registry::ValueType::Binary;
        case REG_DWORD:     return Registry::ValueType::DWord;
        case REG_DWORD_BIG_ENDIAN: return Registry::ValueType::DWordBigEndian;
        case REG_LINK:      return Registry::ValueType::Link;
        case REG_MULTI_SZ:  return Registry::ValueType::MultiString;
        case REG_QWORD:     return Registry::ValueType::QWord;
        default:            return Registry::ValueType::None;
    }
}

// 转换RegistryView到Windows标志
REGSAM ViewToSamFlags(Registry::RegistryView view) {
    switch (view) {
        case Registry::RegistryView::Force32:
            return KEY_WOW64_32KEY;
        case Registry::RegistryView::Force64:
            return KEY_WOW64_64KEY;
        case Registry::RegistryView::Default:
        default:
            // 对于 HKEY_CURRENT_USER，默认应添加视图标志
            #ifdef _WIN64
                return KEY_WOW64_64KEY; // 64位应用默认
            #else
                return KEY_WOW64_32KEY; // 32位应用默认
            #endif
            return 0;
    }
}

// 安全打开注册表键
bool SafeOpenKey(HKEY root, const std::wstring& subkey, REGSAM baseAccess, 
                 Registry::RegistryView view, HKEY* result) {
    REGSAM access = baseAccess | ViewToSamFlags(view);
    LSTATUS status = RegOpenKeyExW(root, subkey.c_str(), 0, access, result);
    return status == ERROR_SUCCESS;
}

// 安全创建注册表键
bool SafeCreateKey(HKEY root, const std::wstring& subkey, REGSAM baseAccess, 
                   Registry::RegistryView view, HKEY* result) {
    REGSAM access = baseAccess | ViewToSamFlags(view);
    DWORD disposition;
    LSTATUS status = RegCreateKeyExW(
        root, 
        subkey.c_str(), 
        0, 
        nullptr, 
        REG_OPTION_NON_VOLATILE, 
        access, 
        nullptr, 
        result, 
        &disposition
    );
    return status == ERROR_SUCCESS;
}

} // 匿名命名空间

bool Registry::KeyExists(RootKey root, const std::string& subkey, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    HKEY hKey = nullptr;
    bool exists = SafeOpenKey(hRoot, wsubkey, KEY_READ, view, &hKey);
    
    if (hKey) {
        RegCloseKey(hKey);
    }
    return exists;
}

bool Registry::CreateKey(RootKey root, const std::string& subkey, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    HKEY hKey = nullptr;
    bool success = SafeCreateKey(hRoot, wsubkey, KEY_WRITE, view, &hKey);
    
    if (hKey) {
        RegCloseKey(hKey);
    }
    return success;
}

bool Registry::DeleteKey(RootKey root, const std::string& subkey, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    // 找到父键
    size_t pos = subkey.find_last_of('\\');
    if (pos == std::string::npos) {
        // 直接删除根键下的子键
        std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
        
        // 使用指定视图删除键
        REGSAM access = KEY_WRITE | ViewToSamFlags(view);
        LSTATUS status = RegDeleteKeyExW(hRoot, wsubkey.c_str(), access, 0);
        return status == ERROR_SUCCESS;
    }
    
    // 打开父键
    std::string parent = subkey.substr(0, pos);
    std::string child = subkey.substr(pos + 1);
    
    std::wstring wparent = WindowsUtils::UTF8ToWide(parent);
    std::wstring wchild = WindowsUtils::UTF8ToWide(child);
    
    HKEY hParent = nullptr;
    if (!SafeOpenKey(hRoot, wparent, KEY_WRITE, view, &hParent)) {
        return false;
    }
    
    // 使用指定视图删除子键
    REGSAM access = ViewToSamFlags(view);
    LSTATUS status = RegDeleteKeyExW(hParent, wchild.c_str(), access, 0);
    RegCloseKey(hParent);
    
    return status == ERROR_SUCCESS;
}

bool Registry::DeleteValue(RootKey root, const std::string& subkey, 
                          const std::string& value, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    std::wstring wvalue = WindowsUtils::UTF8ToWide(value);
    
    HKEY hKey = nullptr;
    if (!SafeOpenKey(hRoot, wsubkey, KEY_WRITE, view, &hKey)) {
        return false;
    }
    
    LSTATUS status = RegDeleteValueW(hKey, wvalue.c_str());
    RegCloseKey(hKey);
    
    return status == ERROR_SUCCESS;
}

bool Registry::ReadString(RootKey root, const std::string& subkey, 
                         const std::string& value, std::string& result, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    std::wstring wvalue = WindowsUtils::UTF8ToWide(value);
    
    HKEY hKey = nullptr;
    if (!SafeOpenKey(hRoot, wsubkey, KEY_READ, view, &hKey)) {
        return false;
    }
    
    DWORD type = 0;
    DWORD size = 0;
    LSTATUS status = RegQueryValueExW(hKey, wvalue.c_str(), nullptr, &type, nullptr, &size);
    
    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(hKey);
        return false;
    }
    
    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1);
    status = RegQueryValueExW(hKey, wvalue.c_str(), nullptr, &type, 
                             reinterpret_cast<LPBYTE>(buffer.data()), &size);
    
    RegCloseKey(hKey);
    
    if (status != ERROR_SUCCESS) {
        return false;
    }
    
    buffer[buffer.size() - 1] = L'\0'; // 确保以null结尾
    result = WindowsUtils::WideToUTF8(buffer.data());
    return true;
}

bool Registry::WriteString(RootKey root, const std::string& subkey,
                          const std::string& value, const std::string& data, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    std::wstring wvalue = WindowsUtils::UTF8ToWide(value);
    std::wstring wdata = WindowsUtils::UTF8ToWide(data);
    
    HKEY hKey = nullptr;
    if (!SafeCreateKey(hRoot, wsubkey, KEY_WRITE, view, &hKey)) {
        return false;
    }
    
    // 包括null终止符
    DWORD size = static_cast<DWORD>((wdata.size() + 1) * sizeof(wchar_t));
    LSTATUS status = RegSetValueExW(hKey, wvalue.c_str(), 0, REG_SZ, 
                                  reinterpret_cast<const BYTE*>(wdata.c_str()), size);
    
    RegCloseKey(hKey);
    return status == ERROR_SUCCESS;
}

bool Registry::ReadDWord(RootKey root, const std::string& subkey,
                        const std::string& value, uint32_t& result, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    std::wstring wvalue = WindowsUtils::UTF8ToWide(value);
    
    HKEY hKey = nullptr;
    if (!SafeOpenKey(hRoot, wsubkey, KEY_READ, view, &hKey)) {
        return false;
    }
    
    DWORD type = 0;
    DWORD data = 0;
    DWORD size = sizeof(data);
    
    LSTATUS status = RegQueryValueExW(hKey, wvalue.c_str(), nullptr, &type, 
                                    reinterpret_cast<LPBYTE>(&data), &size);
    
    RegCloseKey(hKey);
    
    if (status != ERROR_SUCCESS || (type != REG_DWORD && type != REG_DWORD_BIG_ENDIAN)) {
        return false;
    }
    
    result = data;
    return true;
}

bool Registry::WriteDWord(RootKey root, const std::string& subkey,
                         const std::string& value, uint32_t data, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    std::wstring wvalue = WindowsUtils::UTF8ToWide(value);
    
    HKEY hKey = nullptr;
    if (!SafeCreateKey(hRoot, wsubkey, KEY_WRITE, view, &hKey)) {
        return false;
    }
    
    DWORD dwData = static_cast<DWORD>(data);
    LSTATUS status = RegSetValueExW(hKey, wvalue.c_str(), 0, REG_DWORD, 
                                  reinterpret_cast<const BYTE*>(&dwData), sizeof(dwData));
    
    RegCloseKey(hKey);
    return status == ERROR_SUCCESS;
}

bool Registry::ReadQWord(RootKey root, const std::string& subkey,
                        const std::string& value, uint64_t& result, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    std::wstring wvalue = WindowsUtils::UTF8ToWide(value);
    
    HKEY hKey = nullptr;
    if (!SafeOpenKey(hRoot, wsubkey, KEY_READ, view, &hKey)) {
        return false;
    }
    
    DWORD type = 0;
    uint64_t data = 0;
    DWORD size = sizeof(data);
    
    LSTATUS status = RegQueryValueExW(hKey, wvalue.c_str(), nullptr, &type, 
                                    reinterpret_cast<LPBYTE>(&data), &size);
    
    RegCloseKey(hKey);
    
    if (status != ERROR_SUCCESS || type != REG_QWORD) {
        return false;
    }
    
    result = data;
    return true;
}

bool Registry::WriteQWord(RootKey root, const std::string& subkey,
                         const std::string& value, uint64_t data, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    std::wstring wvalue = WindowsUtils::UTF8ToWide(value);
    
    HKEY hKey = nullptr;
    if (!SafeCreateKey(hRoot, wsubkey, KEY_WRITE, view, &hKey)) {
        return false;
    }
    
    LSTATUS status = RegSetValueExW(hKey, wvalue.c_str(), 0, REG_QWORD, 
                                  reinterpret_cast<const BYTE*>(&data), sizeof(data));
    
    RegCloseKey(hKey);
    return status == ERROR_SUCCESS;
}

bool Registry::ReadBinary(RootKey root, const std::string& subkey,
                         const std::string& value, std::vector<uint8_t>& result, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    std::wstring wvalue = WindowsUtils::UTF8ToWide(value);
    
    HKEY hKey = nullptr;
    if (!SafeOpenKey(hRoot, wsubkey, KEY_READ, view, &hKey)) {
        return false;
    }
    
    DWORD type = 0;
    DWORD size = 0;
    LSTATUS status = RegQueryValueExW(hKey, wvalue.c_str(), nullptr, &type, nullptr, &size);
    
    if (status != ERROR_SUCCESS || type != REG_BINARY) {
        RegCloseKey(hKey);
        return false;
    }
    
    result.resize(size);
    status = RegQueryValueExW(hKey, wvalue.c_str(), nullptr, &type, 
                            result.data(), &size);
    
    RegCloseKey(hKey);
    return status == ERROR_SUCCESS;
}

bool Registry::WriteBinary(RootKey root, const std::string& subkey,
                          const std::string& value, const std::vector<uint8_t>& data, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    std::wstring wvalue = WindowsUtils::UTF8ToWide(value);
    
    HKEY hKey = nullptr;
    if (!SafeCreateKey(hRoot, wsubkey, KEY_WRITE, view, &hKey)) {
        return false;
    }
    
    LSTATUS status = RegSetValueExW(hKey, wvalue.c_str(), 0, REG_BINARY, 
                                  data.data(), static_cast<DWORD>(data.size()));
    
    RegCloseKey(hKey);
    return status == ERROR_SUCCESS;
}

bool Registry::EnumSubKeys(RootKey root, const std::string& subkey,
                          std::vector<std::string>& result, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    
    HKEY hKey = nullptr;
    if (!SafeOpenKey(hRoot, wsubkey, KEY_ENUMERATE_SUB_KEYS, view, &hKey)) {
        return false;
    }
    
    result.clear();
    DWORD index = 0;
    wchar_t name[256];
    DWORD nameSize = sizeof(name)/sizeof(wchar_t);
    
    while (RegEnumKeyExW(hKey, index, name, &nameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        result.push_back(WindowsUtils::WideToUTF8(name));
        nameSize = sizeof(name)/sizeof(wchar_t);
        index++;
    }
    
    RegCloseKey(hKey);
    return true;
}

bool Registry::EnumValues(RootKey root, const std::string& subkey,
                         std::vector<ValueInfo>& result, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    
    HKEY hKey = nullptr;
    if (!SafeOpenKey(hRoot, wsubkey, KEY_QUERY_VALUE, view, &hKey)) {
        return false;
    }
    
    result.clear();
    DWORD index = 0;
    wchar_t name[256];
    DWORD nameSize = sizeof(name)/sizeof(wchar_t);
    DWORD type = 0;
    BYTE data[4096];
    DWORD dataSize = sizeof(data);
    
    while (RegEnumValueW(hKey, index, name, &nameSize, nullptr, &type, data, &dataSize) == ERROR_SUCCESS) {
        ValueInfo info;
        info.name = WindowsUtils::WideToUTF8(name);
        info.type = WinRegTypeToValueType(type);
        
        // 复制数据
        if (dataSize > 0) {
            info.data.assign(data, data + dataSize);
        }
        
        result.push_back(std::move(info));
        
        // 重置缓冲区大小
        nameSize = sizeof(name)/sizeof(wchar_t);
        dataSize = sizeof(data);
        index++;
    }
    
    RegCloseKey(hKey);
    return true;
}

} // namespace CorePlatform