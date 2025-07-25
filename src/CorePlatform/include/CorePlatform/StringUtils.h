#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>
#include <regex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include <cctype>
#include <algorithm>
#include <ctime>
#include <unordered_map>
#include "CorePlatform/Export.h"

namespace CorePlatform {

class CORE_PLATFORM_API StringUtils {
public:
    // ====================== 基础字符串操作 ======================
    
    /**
     * 检测字符串是否包含指定子串
     * @param str 源字符串
     * @param substr 要查找的子串
     * @return 如果包含子串返回 true，否则返回 false
     */
    static bool contains(const std::string& str, const std::string& substr);
    
    /**
     * 检测字符串是否包含指定子串（不区分大小写）
     * @param str 源字符串
     * @param substr 要查找的子串
     * @return 如果包含子串返回 true，否则返回 false
     */
    static bool containsIgnoreCase(const std::string& str, const std::string& substr);
    
    /**
     * 删除字符串首尾的空白字符
     * @param str 源字符串
     * @return 修剪后的字符串
     */
    static std::string trim(const std::string& str);
    
    /**
     * 删除字符串左侧的空白字符
     * @param str 源字符串
     * @return 修剪后的字符串
     */
    static std::string trimLeft(const std::string& str);
    
    /**
     * 删除字符串右侧的空白字符
     * @param str 源字符串
     * @return 修剪后的字符串
     */
    static std::string trimRight(const std::string& str);
    
    /**
     * 将字符串转换为小写
     * @param str 源字符串
     * @return 小写字符串
     */
    static std::string toLower(const std::string& str);
    
    /**
     * 将字符串转换为大写
     * @param str 源字符串
     * @return 大写字符串
     */
    static std::string toUpper(const std::string& str);
    
    /**
     * 检查字符串是否以指定前缀开头
     * @param str 源字符串
     * @param prefix 前缀字符串
     * @return 如果以指定前缀开头返回 true，否则返回 false
     */
    static bool startsWith(const std::string& str, const std::string& prefix);
    
    /**
     * 检查字符串是否以指定后缀结尾
     * @param str 源字符串
     * @param suffix 后缀字符串
     * @return 如果以指定后缀结尾返回 true，否则返回 false
     */
    static bool endsWith(const std::string& str, const std::string& suffix);
    
    /**
     * 分割字符串
     * @param str 源字符串
     * @param delimiter 分隔符
     * @return 分割后的字符串向量
     */
    static std::vector<std::string> split(
        const std::string& str, 
        const std::string& delimiter
    );
    
    /**
     * 替换字符串中的子串
     * @param str 源字符串
     * @param from 要替换的子串
     * @param to 替换后的子串
     * @return 替换后的字符串
     */
    static std::string replace(
        const std::string& str, 
        const std::string& from, 
        const std::string& to
    );
    
    // ====================== UTF-8 处理 ======================
    
    /**
     * 严格剔除非UTF-8编码字符
     * @param str 源字符串
     * @return 只包含有效UTF-8字符的字符串
     */
    static std::string removeNonUtf8(const std::string& str);
    
    /**
     * 检查字符串是否为有效的UTF-8编码
     * @param str 源字符串
     * @return 如果是有效UTF-8返回 true，否则返回 false
     */
    static bool isValidUtf8(const std::string& str);
    
    // ====================== 模式提取 ======================
    
    /**
     * 提取第一个匹配的特征子串（匹配即返回）
     * @param str 源字符串
     * @param pattern 正则表达式模式
     * @return 如果找到匹配则返回子串，否则返回 std::nullopt
     */
    static std::optional<std::string> extractFirstPattern(
        const std::string& str, 
        const std::string& pattern
    );
    
    /**
     * 提取所有匹配的特征子串
     * @param str 源字符串
     * @param pattern 正则表达式模式
     * @return 包含所有匹配子串的向量
     */
    static std::vector<std::string> extractPatterns(
        const std::string& str, 
        const std::string& pattern
    );
    
    // ====================== 随机字符串生成 ======================
    
    /**
     * 随机生成指定语言的字符串
     * @param language 语言类型 ("en":英文, "zh":中文, "ru":俄文, "ja":日文, "ar":阿拉伯文)
     * @param minLength 最小长度
     * @param maxLength 最大长度
     * @return 随机生成的字符串
     */
    static std::string randomStringByLanguage(
        const std::string& language = "en", 
        size_t minLength = 5, 
        size_t maxLength = 15
    );
    
    /**
     * 随机生成安全密码
     * @param length 密码长度
     * @param options 密码选项 (可组合使用):
     *   - 'a': 包含小写字母
     *   - 'A': 包含大写字母
     *   - 'd': 包含数字
     *   - 's': 包含特殊字符
     * @return 随机生成的密码
     */
    static std::string generatePassword(
        size_t length = 12,
        const std::string& options = "aAd"
    );
    
    /**
     * 按指定规则生成随机字符串
     * @param length 字符串长度
     * @param charset 自定义字符集
     * @param validator 自定义验证函数 (可选)
     * @return 随机生成的字符串
     */
    static std::string randomString(
        size_t length,
        const std::string& charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
        const std::function<bool(const std::string&)>& validator = nullptr
    );
    
    /**
     * 生成指定格式的随机字符串
     * @param pattern 格式模式 (使用占位符):
     *   - {a}: 小写字母
     *   - {A}: 大写字母
     *   - {d}: 数字
     *   - {s}: 特殊字符
     *   - {*}: 任意可打印字符
     *   - {w}: 字母或数字
     * @return 格式化后的随机字符串
     */
    static std::string randomFormattedString(const std::string& pattern);
    
    /**
     * 生成随机MD5哈希字符串
     * @return 32字符的MD5哈希字符串
     */
    static std::string randomMD5();
    
    /**
     * 生成随机UUID格式签名
     * @return UUID格式字符串
     */
    static std::string randomSignature();
    
    /**
     * 生成随机应用程序清单
     * @return 应用程序清单XML字符串
     */
    static std::string randomAppManifest();
    
    /**
     * 生成重复字符组成的字符串
     * @param c 重复的字符
     * @param length 字符串长度
     * @return 生成的字符串
     */
    static std::string generateString(char c, size_t length);
    
    // ====================== 字符串转换 ======================
    
    /**
     * 十进制转十六进制字符串
     * @param dec 十进制数值
     * @param prefix 是否添加 "0x" 前缀
     * @param minLength 最小长度（不足时前面补零）
     * @return 十六进制字符串
     */
    static std::string decToHex(uint32_t dec, bool prefix = false, int minLength = 0);
    
    // ====================== 格式化与连接 ======================
    
    /**
     * 格式化字符串（类似printf）
     * @param format 格式字符串
     * @param args 可变参数
     * @return 格式化后的字符串
     */
    template<typename... Args>
    static std::string format(const std::string& format, Args... args);
    
    /**
     * 连接多个字符串
     * @param args 可变参数
     * @return 连接后的字符串
     */
    template<typename... Args>
    static std::string join(Args... args);
    
    // ====================== 时间戳 ======================
    
    /**
     * 生成当前时间戳字符串（格式: YYYY-MM-DD HH:MM:SS.mmm）
     * @return 时间戳字符串
     */
    static std::string currentTimestamp();

private:
    // 禁用实例化
    StringUtils() = delete;
    ~StringUtils() = delete;
    
    // UTF-8验证辅助函数
    static bool isLegalUTF8(const unsigned char* sequence, size_t length);
    
    // 语言特定字符集
    static std::string getCharsetForLanguage(const std::string& language);
};

// ====================== 模板函数实现 ======================

template<typename... Args>
std::string StringUtils::format(const std::string& format, Args... args) {
    // 计算所需缓冲区大小
    int size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (size <= 0) {
        return "";
    }
    
    // 分配缓冲区
    std::vector<char> buffer(size);
    snprintf(buffer.data(), size, format.c_str(), args...);
    
    return std::string(buffer.data());
}

template<typename... Args>
std::string StringUtils::join(Args... args) {
    std::ostringstream oss;
    (oss << ... << args);
    return oss.str();
}

} // namespace CorePlatform