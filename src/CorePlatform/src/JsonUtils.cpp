#include "CorePlatform/JsonUtils.h"
#include <sstream>
#include <vector>
#include <cctype>
#include <locale>
#include <codecvt>

namespace CorePlatform {

using json = nlohmann::json;

// UTF-8文件读取（防止编码转换）
std::optional<json> JsonUtils::ParseFromFile(const std::string& filePath) {
    try {
        // 二进制模式打开防止编码转换
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return std::nullopt;
        
        // 读取整个文件
        std::string content((std::istreambuf_iterator<char>(file)), 
                    std::istreambuf_iterator<char>());
        
        return json::parse(content);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<json> JsonUtils::ParseFromString(const std::string& jsonStr) {
    try {
        return json::parse(jsonStr);
    } catch (...) {
        return std::nullopt;
    }
}

bool JsonUtils::IsValid(const json& jsonObj) {
    return !jsonObj.is_discarded();
}

bool JsonUtils::IsValidFile(const std::string& filePath) {
    return ParseFromFile(filePath).has_value();
}

bool JsonUtils::IsValidString(const std::string& jsonStr) {
    return ParseFromString(jsonStr).has_value();
}

// 路径分割（支持中文键名）
std::vector<std::string> JsonUtils::SplitPath(const std::string& path) {
    std::vector<std::string> keys;
    std::string current;
    bool inQuotes = false;
    bool escapeNext = false;
    
    for (char c : path) {
        if (escapeNext) {
            current += c;
            escapeNext = false;
            continue;
        }
        
        switch (c) {
            case '\\': // 转义字符
                escapeNext = true;
                break;
                
            case '"': // 引号处理
                inQuotes = !inQuotes;
                current += c;
                break;
                
            case '.':
                if (!inQuotes) {
                    if (!current.empty()) {
                        keys.push_back(current);
                        current.clear();
                    }
                    continue; // 跳过点号
                }
                // 引号内的点号作为键名部分
                [[fallthrough]];
                
            default:
                current += c;
        }
    }
    
    if (!current.empty()) {
        keys.push_back(current);
    }
    
    return keys;
}

bool JsonUtils::PathExistsImpl(const nlohmann::json& current, 
                              const std::vector<std::string>& keys, 
                              size_t index) {
    if (index >= keys.size()) return true;
    
    const std::string& rawKey = keys[index];
    
    // 处理键名（带引号或不带引号）
    std::string key = rawKey;
    bool isQuoted = false;
    
    // 检查并处理带引号的键名
    if (key.size() >= 2 && key.front() == '"' && key.back() == '"') {
        isQuoted = true;
        key = key.substr(1, key.size() - 2);
        
        // 处理转义引号
        size_t pos = 0;
        while ((pos = key.find("\\\"", pos)) != std::string::npos) {
            key.replace(pos, 2, "\"");
            pos += 1;
        }
    }
    
    // 处理数组索引
    if (current.is_array()) {
        try {
            size_t pos = std::stoul(key);
            return pos < current.size() && 
                   PathExistsImpl(current[pos], keys, index + 1);
        } catch (...) {
            return false;
        }
    }
    // 处理对象键
    else if (current.is_object()) {
        // 1. 首先尝试带引号的原始键名
        if (current.contains(rawKey)) {
            return PathExistsImpl(current[rawKey], keys, index + 1);
        }
        // 2. 尝试处理后的键名（不带引号）
        else if (current.contains(key)) {
            return PathExistsImpl(current[key], keys, index + 1);
        }
        // 3. 回退机制：尝试将当前键作为带点号的复合键名
        else if (!isQuoted) {
            // 合并剩余键名
            std::string compoundKey = key;
            for (size_t i = index + 1; i < keys.size(); i++) {
                compoundKey += '.' + keys[i];
            }
            
            // 检查复合键名是否存在
            if (current.contains(compoundKey)) {
                return true; // 复合键名存在
            }
        }
        return false;
    }
    
    return false;
}

bool JsonUtils::PathExists(const json& jsonObj, const std::string& path) {
    if (path.empty()) return true;
    
    std::vector<std::string> keys = SplitPath(path);
    return !keys.empty() && PathExistsImpl(jsonObj, keys, 0);
}

std::optional<nlohmann::json> JsonUtils::GetValueImpl(const nlohmann::json& jsonObj, 
                                                     const std::vector<std::string>& keys) {
    const nlohmann::json* current = &jsonObj;
    
    for (size_t i = 0; i < keys.size(); i++) {
        const std::string& rawKey = keys[i];
        std::string key = rawKey;
        bool isQuoted = false;
        
        // 处理带引号的键名
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"') {
            isQuoted = true;
            key = key.substr(1, key.size() - 2);
            
            // 处理转义引号
            size_t pos = 0;
            while ((pos = key.find("\\\"", pos)) != std::string::npos) {
                key.replace(pos, 2, "\"");
                pos += 1;
            }
        }
        
        if (current->is_object()) {
            // 1. 尝试带引号的原始键名
            if (current->contains(rawKey)) {
                current = &(*current)[rawKey];
            }
            // 2. 尝试处理后的键名
            else if (current->contains(key)) {
                current = &(*current)[key];
            }
            // 3. 回退机制：尝试复合键名
            else if (!isQuoted) {
                // 合并剩余键名
                std::string compoundKey = key;
                for (size_t j = i + 1; j < keys.size(); j++) {
                    compoundKey += '.' + keys[j];
                }
                
                // 检查复合键名是否存在
                if (current->contains(compoundKey)) {
                    current = &(*current)[compoundKey];
                    break; // 跳过剩余键
                }
                return std::nullopt;
            }
            else {
                return std::nullopt;
            }
        } else if (current->is_array()) {
            try {
                size_t index = std::stoul(key);
                if (index >= current->size()) return std::nullopt;
                current = &(*current)[index];
            } catch (...) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    
    return *current;
}

// 模板方法实现
template<typename T>
std::optional<T> JsonUtils::GetValueByPath(const json& jsonObj, const std::string& path) {
    try {
        std::vector<std::string> keys = SplitPath(path);
        if (keys.empty()) return std::nullopt;
        
        auto result = GetValueImpl(jsonObj, keys);
        if (!result) return std::nullopt;
        
        return result->get<T>();
    } catch (...) {
        return std::nullopt;
    }
}

// 显式实例化常用类型
template std::optional<json> JsonUtils::GetValueByPath(const json&, const std::string&);
template std::optional<std::string> JsonUtils::GetValueByPath(const json&, const std::string&);
template std::optional<int> JsonUtils::GetValueByPath(const json&, const std::string&);
template std::optional<double> JsonUtils::GetValueByPath(const json&, const std::string&);
template std::optional<bool> JsonUtils::GetValueByPath(const json&, const std::string&);
template std::optional<unsigned> JsonUtils::GetValueByPath(const json&, const std::string&);
template std::optional<int64_t> JsonUtils::GetValueByPath(const json&, const std::string&);

// UTF-8文件写入（确保中文正常）
bool JsonUtils::WriteToFile(const std::string& filePath, 
                           const json& jsonObj, 
                           bool pretty,
                           bool ensure_ascii) {
    try {
        // 二进制模式写入防止编码转换
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) return false;
        
        // 控制格式化和中文字符转义
        int indent = pretty ? 4 : -1;
        std::string content = jsonObj.dump(indent, ' ', ensure_ascii);
        
        file.write(content.c_str(), content.size());
        return true;
    } catch (...) {
        return false;
    }
}

// JSON转字符串（支持中文）
std::string JsonUtils::ToString(const json& jsonObj, bool pretty, bool ensure_ascii) {
    int indent = pretty ? 4 : -1;
    return jsonObj.dump(indent, ' ', ensure_ascii);
}

} // namespace CorePlatform