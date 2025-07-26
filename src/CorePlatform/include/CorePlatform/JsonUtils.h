#pragma once

#include "CorePlatform/nlohmann/json.hpp"
#include <string>
#include <optional>
#include <fstream>
#include "CorePlatform/Export.h"

namespace CorePlatform {

class CORE_PLATFORM_API JsonUtils {
public:
    // 从文件解析JSON（UTF-8编码）
    static std::optional<nlohmann::json> ParseFromFile(const std::string& filePath);
    
    // 从字符串解析JSON
    static std::optional<nlohmann::json> ParseFromString(const std::string& jsonStr);
    
    // 检查JSON有效性
    static bool IsValid(const nlohmann::json& jsonObj);
    static bool IsValidFile(const std::string& filePath);
    static bool IsValidString(const std::string& jsonStr);
    
    // 检查路径是否存在（支持中文路径）
    static bool PathExists(const nlohmann::json& jsonObj, const std::string& path);
    
    // 获取路径对应的值（支持中文路径）
    template<typename T = nlohmann::json>
    static std::optional<T> GetValueByPath(const nlohmann::json& jsonObj, const std::string& path);
    
    // 写入JSON文件（UTF-8编码）
    static bool WriteToFile(const std::string& filePath, 
                           const nlohmann::json& jsonObj, 
                           bool pretty = true,
                           bool ensure_ascii = false);
    
    // JSON格式化/压缩（支持中文）
    static std::string ToString(const nlohmann::json& jsonObj, 
                               bool pretty = true, 
                               bool ensure_ascii = false);

private:
    // 路径分割辅助函数（支持中文）
    static std::vector<std::string> SplitPath(const std::string& path);
    
    // 路径存在性检查实现
    static bool PathExistsImpl(const nlohmann::json& current, 
                              const std::vector<std::string>& keys, 
                              size_t index);
    
    // 获取值实现
    static std::optional<nlohmann::json> GetValueImpl(const nlohmann::json& jsonObj, 
                                                     const std::vector<std::string>& keys);
};

} // namespace CorePlatform