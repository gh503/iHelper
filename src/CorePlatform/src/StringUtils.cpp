#include "CorePlatform/StringUtils.h"
#include <cstring>
#include <regex>
#include <random>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <ctime>
#include <unordered_map>
#include <array>

// 如果项目包含OpenSSL，可以启用MD5功能
#ifdef HAS_OPENSSL
#include <openssl/md5.h>
#else
// MD5的替代实现（如果不需要OpenSSL）
static const char* HEX_CHARS = "0123456789abcdef";
#endif

namespace CorePlatform {

// ====================== 基础字符串操作 ======================

bool StringUtils::contains(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}

bool StringUtils::containsIgnoreCase(const std::string& str, const std::string& substr) {
    std::string lowerStr = toLower(str);
    std::string lowerSub = toLower(substr);
    return lowerStr.find(lowerSub) != std::string::npos;
}

std::string StringUtils::trim(const std::string& str) {
    if (str.empty()) return str;
    
    size_t start = 0;
    size_t end = str.size() - 1;
    
    while (start <= end && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }
    
    while (end >= start && std::isspace(static_cast<unsigned char>(str[end]))) {
        end--;
    }
    
    return str.substr(start, end - start + 1);
}

std::string StringUtils::trimLeft(const std::string& str) {
    if (str.empty()) return str;
    
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }
    
    return str.substr(start);
}

std::string StringUtils::trimRight(const std::string& str) {
    if (str.empty()) return str;
    
    size_t end = str.size() - 1;
    while (end > 0 && std::isspace(static_cast<unsigned char>(str[end]))) {
        end--;
    }
    
    return str.substr(0, end + 1);
}

std::string StringUtils::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c){ return std::tolower(c); });
    return result;
}

std::string StringUtils::toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c){ return std::toupper(c); });
    return result;
}

bool StringUtils::startsWith(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size()) return false;
    return std::equal(prefix.begin(), prefix.end(), str.begin());
}

bool StringUtils::endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

std::vector<std::string> StringUtils::split(
    const std::string& str, 
    const std::string& delimiter
) {
    std::vector<std::string> tokens;
    if (str.empty()) return tokens;
    
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    
    tokens.push_back(str.substr(start));
    return tokens;
}

std::string StringUtils::replace(
    const std::string& str, 
    const std::string& from, 
    const std::string& to
) {
    if (from.empty()) return str;
    
    std::string result = str;
    size_t pos = 0;
    
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    
    return result;
}

// ====================== UTF-8 处理 ======================

bool StringUtils::isLegalUTF8(const unsigned char* sequence, size_t length) {
    if (length == 0) return false;
    
    const unsigned char first = sequence[0];
    if (first < 0x80) return true; // 单字节字符
    
    if (first < 0xC2) return false; // 无效起始字节
    
    if (first < 0xE0) { // 2字节序列
        if (length < 2) return false;
        const unsigned char second = sequence[1];
        return (second & 0xC0) == 0x80;
    }
    
    if (first < 0xF0) { // 3字节序列
        if (length < 3) return false;
        const unsigned char second = sequence[1];
        const unsigned char third = sequence[2];
        
        // 处理UTF-16代理对
        if (first == 0xE0 && (second & 0xE0) == 0x80) return false;
        if (first == 0xED && (second & 0xE0) == 0xA0) return false;
        
        return ((second & 0xC0) == 0x80) && 
               ((third & 0xC0) == 0x80);
    }
    
    if (first < 0xF5) { // 4字节序列
        if (length < 4) return false;
        const unsigned char second = sequence[1];
        const unsigned char third = sequence[2];
        const unsigned char fourth = sequence[3];
        
        // 检查有效范围 (U+10FFFF)
        if (first == 0xF0 && (second & 0xF0) == 0x80) return false;
        if (first == 0xF4 && second >= 0x90) return false;
        
        return ((second & 0xC0) == 0x80) && 
               ((third & 0xC0) == 0x80) && 
               ((fourth & 0xC0) == 0x80);
    }
    
    return false; // 无效起始字节
}

std::string StringUtils::removeNonUtf8(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(str.data());
    size_t length = str.size();
    size_t i = 0;
    
    while (i < length) {
        // 单字节字符 (0xxxxxxx)
        if (bytes[i] <= 0x7F) {
            result.push_back(bytes[i]);
            i++;
            continue;
        }
        
        // 多字节字符
        size_t charLen = 0;
        if ((bytes[i] & 0xE0) == 0xC0) charLen = 2; // 2字节序列
        else if ((bytes[i] & 0xF0) == 0xE0) charLen = 3; // 3字节序列
        else if ((bytes[i] & 0xF8) == 0xF0) charLen = 4; // 4字节序列
        else {
            // 无效序列，跳过单个字节
            i++;
            continue;
        }
        
        // 检查是否有足够的字节
        if (i + charLen > length) {
            i++;
            continue;
        }
        
        // 验证序列有效性
        if (isLegalUTF8(bytes + i, charLen)) {
            result.append(str.substr(i, charLen));
        }
        
        i += charLen;
    }
    
    return result;
}

bool StringUtils::isValidUtf8(const std::string& str) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(str.data());
    size_t length = str.size();
    size_t i = 0;
    
    while (i < length) {
        // 单字节字符 (0xxxxxxx)
        if (bytes[i] <= 0x7F) {
            i++;
            continue;
        }
        
        // 多字节字符
        size_t charLen = 0;
        if ((bytes[i] & 0xE0) == 0xC0) charLen = 2; // 2字节序列
        else if ((bytes[i] & 0xF0) == 0xE0) charLen = 3; // 3字节序列
        else if ((bytes[i] & 0xF8) == 0xF0) charLen = 4; // 4字节序列
        else {
            return false; // 无效起始字节
        }
        
        // 检查是否有足够的字节
        if (i + charLen > length) {
            return false;
        }
        
        // 验证序列有效性
        if (!isLegalUTF8(bytes + i, charLen)) {
            return false;
        }
        
        i += charLen;
    }
    
    return true;
}

// ====================== 模式提取 ======================

std::optional<std::string> StringUtils::extractFirstPattern(
    const std::string& str, 
    const std::string& pattern
) {
    try {
        std::regex regexPattern(pattern);
        std::smatch match;
        
        if (std::regex_search(str, match, regexPattern)) {
            return match.str();
        }
    } catch (const std::regex_error& e) {
        // 处理无效的正则表达式
        return std::nullopt;
    }
    
    return std::nullopt;
}

std::vector<std::string> StringUtils::extractPatterns(
    const std::string& str, 
    const std::string& pattern
) {
    std::vector<std::string> matches;
    try {
        std::regex regexPattern(pattern);
        std::sregex_iterator it(str.begin(), str.end(), regexPattern);
        std::sregex_iterator end;
        
        while (it != end) {
            matches.push_back(it->str());
            ++it;
        }
    } catch (const std::regex_error& e) {
        // 处理无效的正则表达式
    }
    return matches;
}

// ====================== 随机字符串生成 ======================

std::string StringUtils::getCharsetForLanguage(const std::string& language) {
    // 定义不同语言的字符集
    static const std::unordered_map<std::string, std::string> languageCharsets = {
        {"en", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"}, // 英文
        {"zh", "的一是在不了有和人这中大为上个国我以要他时来用们生到作地于出就分对成会可主发年动同工也能下过子说产种面而方后多定行学法所民得经十三之进着等部度家电力里如水化高自二理起小物现实加量都两体制机当使点从业本去把性好应开它合还因由其些然前外天政四日那社义事平形相全表间样与关各重新线内数正心反你明看原又么利比或但质气第向道命此变条只没结解问意建月公无系军很情者最立代想已通并提直题党程展五果料象员革位入常文总次品式活设及管特件长求老头基资边流路级少图山统接知较将组见计别她手角期根论运农指几九区强放决西被干做必战先回则任取据处队南给色光门即保治北造百规热领七海口东导器压志世金增争济阶油思术极交受联什认六共权收证改清己美再采转更单风切打白教速花带安场身车例真务具万每目至达走积示议声报斗完类八离华名确才科张信马节话米整空元况今集温传土许步群广石记需段研界拉林律叫且究观越织装"},
        {"ru", "абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"}, // 俄文
        {"ja", "あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをんアイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲン"}, // 日文
        {"ar", "ابتثجحخدذرزسشصضطظعغفقكلمنهوي"}, // 阿拉伯文
        {"ko", "가나다라마바사아자차카타파하"}, // 韩文
        {"hi", "अआइईउऊऋएऐओऔकखगघचछजझटठडढणतथदधनपफबभमयरलवशषसह"} // 印地文
    };
    
    // 默认返回英文字符集
    auto it = languageCharsets.find(language);
    if (it != languageCharsets.end()) {
        return it->second;
    }
    return languageCharsets.at("en");
}

std::string StringUtils::randomStringByLanguage(
    const std::string& language, 
    size_t minLength, 
    size_t maxLength
) {
    static thread_local std::mt19937 generator(std::random_device{}());
    
    // 获取字符集
    std::string charset = getCharsetForLanguage(language);
    
    // 确定长度
    std::uniform_int_distribution<size_t> lengthDist(minLength, maxLength);
    size_t length = lengthDist(generator);
    
    // 生成随机字符串
    std::uniform_int_distribution<size_t> charDist(0, charset.size() - 1);
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[charDist(generator)];
    }
    
    return result;
}

std::string StringUtils::generatePassword(
    size_t length,
    const std::string& options
) {
    // 定义字符集
    std::string lowercase = "abcdefghijklmnopqrstuvwxyz";
    std::string uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string digits = "0123456789";
    std::string symbols = "!@#$%^&*()_-+=<>?/{}[]~";
    
    // 构建完整字符集
    std::string fullCharset;
    if (options.find('a') != std::string::npos) fullCharset += lowercase;
    if (options.find('A') != std::string::npos) fullCharset += uppercase;
    if (options.find('d') != std::string::npos) fullCharset += digits;
    if (options.find('s') != std::string::npos) fullCharset += symbols;
    
    // 如果没有选择任何选项，使用默认字符集
    if (fullCharset.empty()) {
        fullCharset = lowercase + uppercase + digits;
    }
    
    // 生成密码
    return randomString(length, fullCharset, [&](const std::string& pwd) {
        // 验证密码强度
        bool hasLower = false, hasUpper = false, hasDigit = false, hasSymbol = false;
        
        for (char c : pwd) {
            if (options.find('a') != std::string::npos && lowercase.find(c) != std::string::npos) 
                hasLower = true;
            if (options.find('A') != std::string::npos && uppercase.find(c) != std::string::npos) 
                hasUpper = true;
            if (options.find('d') != std::string::npos && digits.find(c) != std::string::npos) 
                hasDigit = true;
            if (options.find('s') != std::string::npos && symbols.find(c) != std::string::npos) 
                hasSymbol = true;
        }
        
        // 检查是否满足所有选中的要求
        if (options.find('a') != std::string::npos && !hasLower) return false;
        if (options.find('A') != std::string::npos && !hasUpper) return false;
        if (options.find('d') != std::string::npos && !hasDigit) return false;
        if (options.find('s') != std::string::npos && !hasSymbol) return false;
        
        return true;
    });
}

std::string StringUtils::randomString(
    size_t length,
    const std::string& charset,
    const std::function<bool(const std::string&)>& validator
) {
    if (charset.empty()) {
        return "";
    }
    
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<size_t> distribution(0, charset.size() - 1);
    
    std::string result;
    result.reserve(length);
    
    // 最大尝试次数，防止无限循环
    const size_t maxAttempts = 100;
    size_t attempts = 0;
    
    do {
        result.clear();
        for (size_t i = 0; i < length; ++i) {
            result += charset[distribution(generator)];
        }
        
        // 如果没有验证器或验证通过，则返回
        if (!validator || validator(result)) {
            return result;
        }
        
        attempts++;
    } while (attempts < maxAttempts);
    
    // 如果无法生成符合要求的字符串，返回最后一次生成的字符串
    return result;
}

std::string StringUtils::randomFormattedString(const std::string& pattern) {
    // 定义字符集
    std::string lowercase = "abcdefghijklmnopqrstuvwxyz";
    std::string uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string digits = "0123456789";
    std::string symbols = "!@#$%^&*()_-+=<>?/{}[]~";
    std::string alphanumeric = lowercase + uppercase + digits;
    std::string printable = alphanumeric + symbols + " ";
    
    static thread_local std::mt19937 generator(std::random_device{}());
    
    std::string result;
    result.reserve(pattern.size() * 2); // 预留足够空间
    
    size_t i = 0;
    while (i < pattern.size()) {
        if (pattern[i] == '{' && i + 1 < pattern.size()) {
            // 处理占位符
            size_t end = pattern.find('}', i);
            if (end != std::string::npos) {
                std::string placeholder = pattern.substr(i + 1, end - i - 1);
                i = end + 1;
                
                // 根据占位符类型选择字符集
                std::string charset;
                if (placeholder == "a") charset = lowercase;
                else if (placeholder == "A") charset = uppercase;
                else if (placeholder == "d") charset = digits;
                else if (placeholder == "s") charset = symbols;
                else if (placeholder == "*") charset = printable;
                else if (placeholder == "w") charset = alphanumeric;
                else charset = placeholder; // 自定义字符集
                
                // 生成随机字符
                if (!charset.empty()) {
                    std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);
                    result += charset[dist(generator)];
                }
                
                continue;
            }
        }
        
        // 普通字符
        result += pattern[i];
        i++;
    }
    
    return result;
}

std::string StringUtils::randomMD5() {
    // 生成16字节随机数据
    std::string randomData = randomString(16, "0123456789abcdef");
    
#ifdef HAS_OPENSSL
    // 使用OpenSSL计算MD5
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(randomData.c_str()), 
        randomData.size(), digest);
    
    // 转换为十六进制字符串
    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(digest[i]);
    }
    return ss.str();
#else
    // 简单的替代实现（不是真正的MD5）
    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, 15);
        ss << HEX_CHARS[dist(gen)];
    }
    return ss.str();
#endif
}

std::string StringUtils::randomSignature() {
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution(0, 15);
    std::uniform_int_distribution<int> distribution2(8, 11);
    
    std::stringstream ss;
    
    // 生成UUID格式: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    for (int i = 0; i < 32; i++) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            ss << "-";
        }
        
        if (i == 12) {
            ss << "4"; // UUID版本4
        } else if (i == 16) {
            ss << std::hex << distribution2(generator); // 8,9,A,B
        } else {
            ss << std::hex << distribution(generator);
        }
    }
    
    return ss.str();
}

std::string StringUtils::randomAppManifest() {
    static const std::vector<std::string> appNames = {
        "MyApp", "SuperTool", "DataProcessor", "CloudService", "DesktopUtility",
        "FileManager", "ImageEditor", "VideoPlayer", "MusicStreamer", "GameLauncher"
    };
    
    static const std::vector<std::string> publishers = {
        "TechCorp", "InnovateInc", "DigitalSolutions", "FutureTech", "CodeMasters",
        "SoftwareGurus", "AppFactory", "DevTeam", "OpenSourceOrg", "EnterpriseSoft"
    };
    
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<size_t> distName(0, appNames.size() - 1);
    std::uniform_int_distribution<size_t> distPub(0, publishers.size() - 1);
    std::uniform_int_distribution<int> distVersion(1, 20);
    std::uniform_int_distribution<int> distBuild(100, 9999);
    
    std::string appName = appNames[distName(generator)];
    std::string publisher = publishers[distPub(generator)];
    int major = distVersion(generator);
    int minor = distVersion(generator);
    int build = distBuild(generator);
    int revision = distBuild(generator);
    
    std::stringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
       << "<assembly manifestVersion=\"1.0\" xmlns=\"urn:schemas-microsoft-com:asm.v1\">\n"
       << "  <assemblyIdentity version=\"" 
       << major << "." << minor << "." << build << "." << revision << "\"\n"
       << "    name=\"" << publisher << "." << appName << "\"\n"
       << "    type=\"win32\"\n"
       << "    processorArchitecture=\"*\" />\n"
       << "  <description>" << appName << " Application</description>\n"
       << "  <dependency>\n"
       << "    <dependentAssembly>\n"
       << "      <assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" "
       << "version=\"6.0.0.0\" processorArchitecture=\"*\" "
       << "publicKeyToken=\"6595b64144ccf1df\" language=\"*\" />\n"
       << "    </dependentAssembly>\n"
       << "  </dependency>\n"
       << "  <application>\n"
       << "    <windowsSettings>\n"
       << "      <dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">true</dpiAware>\n"
       << "    </windowsSettings>\n"
       << "  </application>\n"
       << "</assembly>";
    
    return ss.str();
}

std::string StringUtils::generateString(char c, size_t length) {
    return std::string(length, c);
}

// ====================== 字符串转换 ======================

std::string StringUtils::decToHex(uint32_t dec, bool prefix, int minLength) {
    std::stringstream ss;
    ss << std::hex << dec;
    std::string hexStr = ss.str();
    
    // 添加前导零
    if (hexStr.length() < static_cast<size_t>(minLength)) {
        hexStr = std::string(minLength - hexStr.length(), '0') + hexStr;
    }
    
    // 添加前缀
    if (prefix) {
        hexStr = "0x" + hexStr;
    }
    
    return hexStr;
}

// ====================== 时间戳 ======================

std::string StringUtils::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

} // namespace CorePlatform