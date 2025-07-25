#include "CorePlatform/StringUtils.h"
#include <gtest/gtest.h>
#include <vector>
#include <cctype>
#include <regex>

using namespace CorePlatform;

// ====================== 基础字符串操作测试 ======================

TEST(StringUtilsTest, Contains) {
    std::string text = "The quick brown fox jumps over the lazy dog";
    EXPECT_TRUE(StringUtils::contains(text, "fox"));
    EXPECT_FALSE(StringUtils::contains(text, "cat"));
    EXPECT_TRUE(StringUtils::contains(text, ""));
}

TEST(StringUtilsTest, ContainsIgnoreCase) {
    std::string text = "Hello World!";
    EXPECT_TRUE(StringUtils::containsIgnoreCase(text, "hello"));
    EXPECT_TRUE(StringUtils::containsIgnoreCase(text, "WORLD"));
    EXPECT_FALSE(StringUtils::containsIgnoreCase(text, "earth"));
}

TEST(StringUtilsTest, Trim) {
    EXPECT_EQ(StringUtils::trim("   Hello World   "), "Hello World");
    EXPECT_EQ(StringUtils::trim("\t\nHello\t\n"), "Hello");
    EXPECT_EQ(StringUtils::trim(""), "");
    EXPECT_EQ(StringUtils::trim("NoSpaces"), "NoSpaces");
}

TEST(StringUtilsTest, TrimLeft) {
    EXPECT_EQ(StringUtils::trimLeft("   Hello"), "Hello");
    EXPECT_EQ(StringUtils::trimLeft("\t\nHello"), "Hello");
    EXPECT_EQ(StringUtils::trimLeft("Hello   "), "Hello   ");
}

TEST(StringUtilsTest, TrimRight) {
    EXPECT_EQ(StringUtils::trimRight("Hello   "), "Hello");
    EXPECT_EQ(StringUtils::trimRight("Hello\t\n"), "Hello");
    EXPECT_EQ(StringUtils::trimRight("   Hello"), "   Hello");
}

TEST(StringUtilsTest, ToLower) {
    EXPECT_EQ(StringUtils::toLower("Hello World!"), "hello world!");
    EXPECT_EQ(StringUtils::toLower("123ABC"), "123abc");
    EXPECT_EQ(StringUtils::toLower(""), "");
}

TEST(StringUtilsTest, ToUpper) {
    EXPECT_EQ(StringUtils::toUpper("Hello World!"), "HELLO WORLD!");
    EXPECT_EQ(StringUtils::toUpper("123abc"), "123ABC");
    EXPECT_EQ(StringUtils::toUpper(""), "");
}

TEST(StringUtilsTest, StartsWith) {
    EXPECT_TRUE(StringUtils::startsWith("Hello World", "Hello"));
    EXPECT_FALSE(StringUtils::startsWith("Hello World", "World"));
    EXPECT_TRUE(StringUtils::startsWith("", ""));
    EXPECT_FALSE(StringUtils::startsWith("Short", "TooLong"));
}

TEST(StringUtilsTest, EndsWith) {
    EXPECT_TRUE(StringUtils::endsWith("Hello World", "World"));
    EXPECT_FALSE(StringUtils::endsWith("Hello World", "Hello"));
    EXPECT_TRUE(StringUtils::endsWith("", ""));
    EXPECT_FALSE(StringUtils::endsWith("Short", "TooLong"));
}

TEST(StringUtilsTest, Split) {
    std::vector<std::string> expected = {"a", "b", "c"};
    EXPECT_EQ(StringUtils::split("a,b,c", ","), expected);
    
    expected = {"apple", "banana", "orange"};
    EXPECT_EQ(StringUtils::split("apple,banana,orange", ","), expected);
    
    expected = {"a", "b", "c"};
    EXPECT_EQ(StringUtils::split("a b c", " "), expected);
    
    expected = {"abc"};
    EXPECT_EQ(StringUtils::split("abc", ","), expected);
    
    expected = {"", "a", "b"};
    EXPECT_EQ(StringUtils::split(",a,b", ","), expected);
}

TEST(StringUtilsTest, Replace) {
    EXPECT_EQ(StringUtils::replace("Hello World", "World", "Universe"), "Hello Universe");
    EXPECT_EQ(StringUtils::replace("aaa", "a", "b"), "bbb");
    EXPECT_EQ(StringUtils::replace("abc", "", "x"), "abc");
    EXPECT_EQ(StringUtils::replace("", "a", "b"), "");
    EXPECT_EQ(StringUtils::replace("aabbcc", "bb", "dd"), "aaddcc");
}

// ====================== UTF-8 处理测试 ======================

TEST(StringUtilsTest, RemoveNonUtf8) {
    // 有效UTF-8字符串
    std::string valid = "你好，世界！";
    EXPECT_EQ(StringUtils::removeNonUtf8(valid), valid);
    
    // 包含无效字节的字符串
    std::string invalid = "Valid: 你好, Invalid: \x80\xFF";
    std::string cleaned = StringUtils::removeNonUtf8(invalid);
    EXPECT_TRUE(StringUtils::isValidUtf8(cleaned));
    EXPECT_EQ(cleaned, "Valid: 你好, Invalid: ");
    
    // 边界情况
    EXPECT_TRUE(StringUtils::removeNonUtf8("").empty());
    
    // 部分有效序列
    std::string partial = "\xC3\x28"; // 无效序列
    EXPECT_TRUE(StringUtils::removeNonUtf8(partial).empty());
}

TEST(StringUtilsTest, IsValidUtf8) {
    EXPECT_TRUE(StringUtils::isValidUtf8(""));
    EXPECT_TRUE(StringUtils::isValidUtf8("Hello World"));
    EXPECT_TRUE(StringUtils::isValidUtf8("你好，世界！"));
    
    // 无效序列
    EXPECT_FALSE(StringUtils::isValidUtf8("\xC0\x80")); // 无效2字节序列
    EXPECT_FALSE(StringUtils::isValidUtf8("\xE0\x80\x80")); // 无效3字节序列
    EXPECT_FALSE(StringUtils::isValidUtf8("\xF0\x80\x80\x80")); // 无效4字节序列
    EXPECT_FALSE(StringUtils::isValidUtf8("\xFF")); // 无效起始字节
    EXPECT_FALSE(StringUtils::isValidUtf8("\x80")); // 无效后续字节
}

// ====================== 模式提取测试 ======================

TEST(StringUtilsTest, ExtractFirstPattern) {
    std::string text = "Contact us at support@example.com or sales@domain.com";
    
    // 提取第一个邮箱
    auto email = StringUtils::extractFirstPattern(text, R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b)");
    EXPECT_TRUE(email.has_value());
    EXPECT_EQ(email.value(), "support@example.com");
    
    // 无效正则
    auto invalid = StringUtils::extractFirstPattern(text, "[invalid(regex");
    EXPECT_FALSE(invalid.has_value());
    
    // 无匹配
    auto noMatch = StringUtils::extractFirstPattern(text, "\\d{20}");
    EXPECT_FALSE(noMatch.has_value());
}

TEST(StringUtilsTest, ExtractPatterns) {
    std::string text = "Emails: john@example.com, mary@domain.com; Phone: +1-555-1234";
    
    // 提取所有邮箱
    auto emails = StringUtils::extractPatterns(text, R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b)");
    ASSERT_EQ(emails.size(), 2);
    EXPECT_EQ(emails[0], "john@example.com");
    EXPECT_EQ(emails[1], "mary@domain.com");
    
    // 提取电话号码
    auto phones = StringUtils::extractPatterns(text, R"(\+\d{1,3}-\d{3}-\d{4})");
    ASSERT_EQ(phones.size(), 1);
    EXPECT_EQ(phones[0], "+1-555-1234");
    
    // 无匹配
    auto noMatch = StringUtils::extractPatterns(text, "\\d{20}");
    EXPECT_TRUE(noMatch.empty());
}

// ====================== 随机字符串生成测试 ======================

TEST(StringUtilsTest, RandomStringByLanguage) {
    // 英文
    std::string enStr = StringUtils::randomStringByLanguage("en", 5, 10);
    EXPECT_GE(enStr.size(), 5);
    EXPECT_LE(enStr.size(), 10);
    for (char c : enStr) {
        EXPECT_TRUE(std::isalpha(c) || std::isspace(c));
    }
    
    // 中文
    std::string zhStr = StringUtils::randomStringByLanguage("zh", 3, 6);
    EXPECT_GE(zhStr.size(), 3);
    EXPECT_LE(zhStr.size(), 6);
    // 中文字符通常占用3个字节
    EXPECT_GE(zhStr.size(), 3); // 至少1个字符
    
    // 无效语言
    std::string invalid = StringUtils::randomStringByLanguage("xx", 5, 5);
    EXPECT_EQ(invalid.size(), 5);
}

TEST(StringUtilsTest, GeneratePassword) {
    // 默认密码
    std::string defaultPwd = StringUtils::generatePassword();
    EXPECT_EQ(defaultPwd.size(), 12);
    
    // 强密码
    std::string strongPwd = StringUtils::generatePassword(16, "aAds");
    EXPECT_EQ(strongPwd.size(), 16);
    
    // 验证字符类型
    bool hasLower = false, hasUpper = false, hasDigit = false, hasSymbol = false;
    for (char c : strongPwd) {
        if (std::islower(c)) hasLower = true;
        else if (std::isupper(c)) hasUpper = true;
        else if (std::isdigit(c)) hasDigit = true;
        else hasSymbol = true;
    }
    EXPECT_TRUE(hasLower);
    EXPECT_TRUE(hasUpper);
    EXPECT_TRUE(hasDigit);
    EXPECT_TRUE(hasSymbol);
    
    // 纯数字密码
    std::string numericPwd = StringUtils::generatePassword(6, "d");
    EXPECT_EQ(numericPwd.size(), 6);
    for (char c : numericPwd) {
        EXPECT_TRUE(std::isdigit(c));
    }
}

TEST(StringUtilsTest, RandomString) {
    // 默认字符集
    std::string defaultStr = StringUtils::randomString(10);
    EXPECT_EQ(defaultStr.size(), 10);
    for (char c : defaultStr) {
        EXPECT_TRUE(std::isalnum(c) || std::isspace(c));
    }
    
    // 自定义字符集
    std::string binary = StringUtils::randomString(8, "01");
    EXPECT_EQ(binary.size(), 8);
    for (char c : binary) {
        EXPECT_TRUE(c == '0' || c == '1');
    }
    
    // 带验证器
    std::string withValidator = StringUtils::randomString(10, "01", [](const std::string& s) {
        return std::count(s.begin(), s.end(), '1') >= 3;
    });
    EXPECT_GE(std::count(withValidator.begin(), withValidator.end(), '1'), 3);
}

TEST(StringUtilsTest, RandomFormattedString) {
    // 简单格式
    std::string simple = StringUtils::randomFormattedString("{a}{A}{d}{s}");
    EXPECT_EQ(simple.size(), 4);
    EXPECT_TRUE(std::islower(simple[0]));
    EXPECT_TRUE(std::isupper(simple[1]));
    EXPECT_TRUE(std::isdigit(simple[2]));
    EXPECT_FALSE(std::isalnum(simple[3]));
    
    // 复杂格式
    std::string license = StringUtils::randomFormattedString("LIC-{A}{A}{A}-{d}{d}{d}{d}");
    EXPECT_GE(license.size(), 11);
    EXPECT_EQ(license.substr(0, 4), "LIC-");
    EXPECT_TRUE(std::isupper(license[4]));
    EXPECT_TRUE(std::isupper(license[5]));
    EXPECT_TRUE(std::isupper(license[6]));
    EXPECT_EQ(license[7], '-');
    EXPECT_TRUE(std::isdigit(license[8]));
    EXPECT_TRUE(std::isdigit(license[9]));
    EXPECT_TRUE(std::isdigit(license[10]));
    EXPECT_TRUE(std::isdigit(license[11]));
    
    // 自定义字符集
    std::string custom = StringUtils::randomFormattedString("{xyz}");
    EXPECT_EQ(custom.size(), 1);
    EXPECT_TRUE(custom == "x" || custom == "y" || custom == "z");
}

TEST(StringUtilsTest, RandomMD5) {
    std::string md5 = StringUtils::randomMD5();
    EXPECT_EQ(md5.size(), 32);
    
    // 验证十六进制字符
    for (char c : md5) {
        EXPECT_TRUE(std::isxdigit(c));
    }
}

TEST(StringUtilsTest, RandomSignature) {
    std::string signature = StringUtils::randomSignature();
    
    // 验证UUID格式: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    EXPECT_EQ(signature.size(), 36);
    EXPECT_EQ(signature[8], '-');
    EXPECT_EQ(signature[13], '-');
    EXPECT_EQ(signature[18], '-');
    EXPECT_EQ(signature[23], '-');
    EXPECT_EQ(signature[14], '4');
    
    // 验证版本位 (y)
    char y = signature[19];
    EXPECT_TRUE(y == '8' || y == '9' || y == 'a' || y == 'b');
}

TEST(StringUtilsTest, RandomAppManifest) {
    std::string manifest = StringUtils::randomAppManifest();
    
    // 验证基本结构
    EXPECT_TRUE(manifest.find("<?xml") != std::string::npos);
    EXPECT_TRUE(manifest.find("<assembly") != std::string::npos);
    EXPECT_TRUE(manifest.find("</assembly>") != std::string::npos);
    
    // 验证版本号格式
    std::regex versionRegex("version=\"\\d+\\.\\d+\\.\\d+\\.\\d+\"");
    EXPECT_TRUE(std::regex_search(manifest, versionRegex));
    
    // 验证依赖项
    EXPECT_TRUE(manifest.find("Microsoft.Windows.Common-Controls") != std::string::npos);
}

TEST(StringUtilsTest, GenerateString) {
    EXPECT_EQ(StringUtils::generateString('a', 5), "aaaaa");
    EXPECT_EQ(StringUtils::generateString('*', 3), "***");
    EXPECT_EQ(StringUtils::generateString(' ', 0), "");
}

// ====================== 字符串转换测试 ======================

TEST(StringUtilsTest, DecToHex) {
    EXPECT_EQ(StringUtils::decToHex(255), "ff");
    EXPECT_EQ(StringUtils::decToHex(255, true), "0xff");
    EXPECT_EQ(StringUtils::decToHex(15, false, 4), "000f");
    EXPECT_EQ(StringUtils::decToHex(0), "0");
    EXPECT_EQ(StringUtils::decToHex(0xABCDEF), "abcdef");
}

// ====================== 格式化与连接测试 ======================

TEST(StringUtilsTest, Format) {
    EXPECT_EQ(StringUtils::format("Hello %s", "World"), "Hello World");
    EXPECT_EQ(StringUtils::format("%d + %d = %d", 2, 3, 5), "2 + 3 = 5");
    EXPECT_EQ(StringUtils::format("Price: $%.2f", 12.345), "Price: $12.35");
    EXPECT_EQ(StringUtils::format(""), "");
    EXPECT_EQ(StringUtils::format("No placeholders"), "No placeholders");
}

TEST(StringUtilsTest, Join) {
    EXPECT_EQ(StringUtils::join("Hello", " ", "World", "!"), "Hello World!");
    EXPECT_EQ(StringUtils::join(1, " + ", 2, " = ", 3), "1 + 2 = 3");
    EXPECT_EQ(StringUtils::join(true, " and ", false), "1 and 0");
    EXPECT_EQ(StringUtils::join(), "");
}

// ====================== 时间戳测试 ======================

TEST(StringUtilsTest, CurrentTimestamp) {
    std::string timestamp = StringUtils::currentTimestamp();
    
    // 验证基本格式: YYYY-MM-DD HH:MM:SS.mmm
    EXPECT_EQ(timestamp.size(), 23);
    EXPECT_EQ(timestamp[4], '-');
    EXPECT_EQ(timestamp[7], '-');
    EXPECT_EQ(timestamp[10], ' ');
    EXPECT_EQ(timestamp[13], ':');
    EXPECT_EQ(timestamp[16], ':');
    EXPECT_EQ(timestamp[19], '.');
    
    // 验证年份范围 (假设测试在2000-2099年运行)
    std::string year = timestamp.substr(0, 4);
    int yearNum = std::stoi(year);
    EXPECT_GE(yearNum, 2000);
    EXPECT_LE(yearNum, 2099);
    
    // 验证毫秒部分
    std::string ms = timestamp.substr(20, 3);
    for (char c : ms) {
        EXPECT_TRUE(std::isdigit(c));
    }
}

// ====================== 性能与边界测试 ======================

TEST(StringUtilsTest, Performance) {
    // 大字符串处理
    std::string largeStr(100000, 'a');
    largeStr += "   ";
    
    // 修剪性能
    auto start = std::chrono::high_resolution_clock::now();
    std::string trimmed = StringUtils::trim(largeStr);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 10) << "Trim performance issue";
    
    // UTF-8处理性能
    std::string largeUtf8 = largeStr + "你好";
    start = std::chrono::high_resolution_clock::now();
    std::string cleaned = StringUtils::removeNonUtf8(largeUtf8);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 50) << "UTF-8 processing performance issue";
}

TEST(StringUtilsTest, EdgeCases) {
    // 空字符串处理
    EXPECT_EQ(StringUtils::trim(""), "");
    EXPECT_EQ(StringUtils::toLower(""), "");
    EXPECT_EQ(StringUtils::split("", ",").size(), 0);
    EXPECT_TRUE(StringUtils::extractFirstPattern("", "pattern") == std::nullopt);
    EXPECT_TRUE(StringUtils::extractPatterns("", "pattern").empty());
    EXPECT_TRUE(StringUtils::isValidUtf8(""));
    EXPECT_EQ(StringUtils::removeNonUtf8(""), "");
    
    // 空字符集
    EXPECT_TRUE(StringUtils::randomString(10, "").empty());
    
    // 零长度
    EXPECT_TRUE(StringUtils::randomString(0).empty());
    EXPECT_TRUE(StringUtils::generatePassword(0).empty());
    EXPECT_TRUE(StringUtils::randomStringByLanguage("en", 0, 0).empty());
}