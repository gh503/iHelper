#include "CorePlatform/JsonUtils.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace CorePlatform;

class JsonUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时目录
        tempDir = fs::temp_directory_path() / "JsonUtilsTest";
        fs::create_directories(tempDir);
        
        // 创建普通测试JSON文件
        testJsonFile = tempDir / "test.json";
        std::ofstream testFile(testJsonFile);
        testFile << R"({
            "name": "test",
            "value": 42,
            "nested": {
                "key": "nested_value",
                "array": [1, 2, 3]
            }
        })";
        testFile.close();
        
        // 创建中文测试JSON文件
        chineseJsonFile = tempDir / "chinese.json";
        std::ofstream chineseFile(chineseJsonFile, std::ios::binary);
        chineseFile << R"({
            "用户": {
                "姓名": "张三",
                "年龄": 30,
                "地址": "北京市",
                "联系方式": [
                    {"类型": "手机", "号码": "13800138000"},
                    {"类型": "邮箱", "地址": "zhangsan@example.com"}
                ]
            },
            "描述": "这是一个测试"
        })";
        chineseFile.close();
    }
    
    void TearDown() override {
        // 删除临时目录
        fs::remove_all(tempDir);
    }
    
    fs::path tempDir;
    fs::path testJsonFile;
    fs::path chineseJsonFile;
};

// 测试从文件解析
TEST_F(JsonUtilsTest, ParseFromFile) {
    auto json = JsonUtils::ParseFromFile(testJsonFile.string());
    ASSERT_TRUE(json.has_value());
    EXPECT_TRUE(JsonUtils::IsValid(*json));
    
    // 测试不存在的文件
    auto invalid = JsonUtils::ParseFromFile(tempDir.string() + "/not_exist.json");
    EXPECT_FALSE(invalid.has_value());
}

// 测试从字符串解析
TEST_F(JsonUtilsTest, ParseFromString) {
    std::string validStr = R"({"key": "value"})";
    auto json = JsonUtils::ParseFromString(validStr);
    ASSERT_TRUE(json.has_value());
    EXPECT_TRUE(JsonUtils::IsValid(*json));
    
    std::string invalidStr = "{key: value}";
    auto invalid = JsonUtils::ParseFromString(invalidStr);
    EXPECT_FALSE(invalid.has_value());
}

// 测试有效性检查
TEST_F(JsonUtilsTest, IsValid) {
    std::string validStr = R"({"key": "value"})";
    auto json = JsonUtils::ParseFromString(validStr);
    ASSERT_TRUE(json.has_value());
    EXPECT_TRUE(JsonUtils::IsValid(*json));
    
    // 测试无效字符串
    std::string invalidStr = "invalid";
    auto invalidJson = JsonUtils::ParseFromString(invalidStr);
    EXPECT_FALSE(invalidJson.has_value());
}

// 测试路径存在性检查
TEST_F(JsonUtilsTest, PathExists) {
    auto json = JsonUtils::ParseFromFile(testJsonFile.string());
    ASSERT_TRUE(json.has_value());
    
    // 存在路径
    EXPECT_TRUE(JsonUtils::PathExists(*json, "name"));
    EXPECT_TRUE(JsonUtils::PathExists(*json, "nested.key"));
    EXPECT_TRUE(JsonUtils::PathExists(*json, "nested.array.0")); // 数组索引
    
    // 不存在路径
    EXPECT_FALSE(JsonUtils::PathExists(*json, "not_exist"));
    EXPECT_FALSE(JsonUtils::PathExists(*json, "nested.not_exist"));
    EXPECT_FALSE(JsonUtils::PathExists(*json, "nested.array.3")); // 越界
    
    // 测试中文路径
    auto chineseJson = JsonUtils::ParseFromFile(chineseJsonFile.string());
    ASSERT_TRUE(chineseJson.has_value());
    EXPECT_TRUE(JsonUtils::PathExists(*chineseJson, "用户.姓名"));
    EXPECT_TRUE(JsonUtils::PathExists(*chineseJson, "用户.联系方式.0.类型"));
    EXPECT_FALSE(JsonUtils::PathExists(*chineseJson, "用户.电话")); // 不存在
}

// 测试按路径获取值
TEST_F(JsonUtilsTest, GetValueByPath) {
    auto json = JsonUtils::ParseFromFile(testJsonFile.string());
    ASSERT_TRUE(json.has_value());
    
    // 获取字符串
    auto name = JsonUtils::GetValueByPath<std::string>(*json, "name");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "test");
    
    // 获取整数
    auto value = JsonUtils::GetValueByPath<int>(*json, "value");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 42);
    
    // 获取嵌套值
    auto nestedValue = JsonUtils::GetValueByPath<std::string>(*json, "nested.key");
    ASSERT_TRUE(nestedValue.has_value());
    EXPECT_EQ(*nestedValue, "nested_value");
    
    // 获取数组元素
    auto arrayElement = JsonUtils::GetValueByPath<int>(*json, "nested.array.1");
    ASSERT_TRUE(arrayElement.has_value());
    EXPECT_EQ(*arrayElement, 2);
    
    // 不存在的路径
    auto notExist = JsonUtils::GetValueByPath<std::string>(*json, "not.exist");
    EXPECT_FALSE(notExist.has_value());
    
    // 测试中文路径获取
    auto chineseJson = JsonUtils::ParseFromFile(chineseJsonFile.string());
    ASSERT_TRUE(chineseJson.has_value());
    auto chineseName = JsonUtils::GetValueByPath<std::string>(*chineseJson, "用户.姓名");
    ASSERT_TRUE(chineseName.has_value());
    EXPECT_EQ(*chineseName, "张三");
    
    // 测试中文数组元素
    auto contactType = JsonUtils::GetValueByPath<std::string>(*chineseJson, "用户.联系方式.0.类型");
    ASSERT_TRUE(contactType.has_value());
    EXPECT_EQ(*contactType, "手机");
}

// 测试写入文件
TEST_F(JsonUtilsTest, WriteToFile) {
    auto json = JsonUtils::ParseFromFile(testJsonFile.string());
    ASSERT_TRUE(json.has_value());
    
    // 写入格式化文件
    fs::path prettyFile = tempDir / "pretty.json";
    EXPECT_TRUE(JsonUtils::WriteToFile(prettyFile.string(), *json));
    
    // 读取并验证
    auto prettyJson = JsonUtils::ParseFromFile(prettyFile.string());
    ASSERT_TRUE(prettyJson.has_value());
    EXPECT_EQ(*json, *prettyJson);
    
    // 写入压缩文件
    fs::path compressedFile = tempDir / "compressed.json";
    EXPECT_TRUE(JsonUtils::WriteToFile(compressedFile.string(), *json, false));
    
    // 读取并验证内容一致
    auto compressedJson = JsonUtils::ParseFromFile(compressedFile.string());
    ASSERT_TRUE(compressedJson.has_value());
    EXPECT_EQ(*json, *compressedJson);
    
    // 测试中文写入
    auto chineseJson = JsonUtils::ParseFromFile(chineseJsonFile.string());
    ASSERT_TRUE(chineseJson.has_value());
    fs::path chineseOutFile = tempDir / "chinese_out.json";
    EXPECT_TRUE(JsonUtils::WriteToFile(chineseOutFile.string(), *chineseJson));
    
    // 检查文件内容是否包含中文字符
    std::ifstream inFile(chineseOutFile, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(inFile)), 
                       std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("张三"), std::string::npos);
    EXPECT_NE(content.find("北京市"), std::string::npos);
}

// 测试转字符串
TEST_F(JsonUtilsTest, ToString) {
    nlohmann::json json = {
        {"key", "value"},
        {"int", 42},
        {"nested", {{"array", {1,2,3}}}}
    };
    
    // 格式化
    std::string pretty = JsonUtils::ToString(json);
    EXPECT_NE(pretty.find('\n'), std::string::npos);
    
    // 压缩
    std::string compressed = JsonUtils::ToString(json, false);
    EXPECT_EQ(compressed.find('\n'), std::string::npos);
    
    // 测试中文
    nlohmann::json chineseJson;
    chineseJson["中文"] = "测试";
    std::string chineseStr = JsonUtils::ToString(chineseJson);
    EXPECT_NE(chineseStr.find("测试"), std::string::npos);
    
    // 测试ensure_ascii=true
    std::string asciiStr = JsonUtils::ToString(chineseJson, true, true);
    EXPECT_EQ(asciiStr.find("测试"), std::string::npos);
    EXPECT_NE(asciiStr.find("\\u6d4b\\u8bd5"), std::string::npos); // "测试"的Unicode转义
}

// 测试中文文件读写
TEST_F(JsonUtilsTest, ChineseFileHandling) {
    // 创建包含中文的JSON对象
    nlohmann::json chineseData;
    chineseData["信息"] = {
        {"姓名", "李四"},
        {"职位", "软件工程师"},
        {"技能", {"C++", "Python", "设计模式"}}
    };
    chineseData["说明"] = "这是一个包含中文的JSON测试";
    
    // 写入文件
    fs::path outFile = tempDir / "chinese_output.json";
    ASSERT_TRUE(JsonUtils::WriteToFile(outFile.string(), chineseData));
    
    // 读取文件
    auto readData = JsonUtils::ParseFromFile(outFile.string());
    ASSERT_TRUE(readData.has_value());
    
    // 验证中文内容
    auto name = JsonUtils::GetValueByPath<std::string>(*readData, "信息.姓名");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "李四");
    
    auto position = JsonUtils::GetValueByPath<std::string>(*readData, "信息.职位");
    ASSERT_TRUE(position.has_value());
    EXPECT_EQ(*position, "软件工程师");
    
    // 验证中文路径
    EXPECT_TRUE(JsonUtils::PathExists(*readData, "信息.技能.0"));
    auto skill = JsonUtils::GetValueByPath<std::string>(*readData, "信息.技能.0");
    ASSERT_TRUE(skill.has_value());
    EXPECT_EQ(*skill, "C++");
    
    // 验证完整内容
    auto description = JsonUtils::GetValueByPath<std::string>(*readData, "说明");
    ASSERT_TRUE(description.has_value());
    EXPECT_EQ(*description, "这是一个包含中文的JSON测试");
}

TEST_F(JsonUtilsTest, NestedPaths) {
    // 创建复杂嵌套结构
    nlohmann::json data = {
        {"a", {
            {"b", {
                {"c", {
                    {"d", "最终值"},
                    {"e", {1, 2, 3}},
                    {"f.g.h", "带点号的键名"} // 嵌套键名带点号
                }}
            }}
        }},
        {"x.y.z", "点号在键名中"} // 顶层键名带点号
    };
    
    // 测试路径存在性
    EXPECT_TRUE(JsonUtils::PathExists(data, "a.b.c.d"));
    EXPECT_TRUE(JsonUtils::PathExists(data, "a.b.c.e.1"));
    
    // 测试带点号的键名 - 使用引号
    EXPECT_TRUE(JsonUtils::PathExists(data, R"(a.b.c."f.g.h")"));
    
    // 测试带点号的键名 - 不使用引号（回退机制）
    EXPECT_TRUE(JsonUtils::PathExists(data, "a.b.c.f.g.h"));
    
    // 测试不存在的路径
    EXPECT_FALSE(JsonUtils::PathExists(data, "a.b.c.f"));
    
    // 测试获取值 - 标准方式
    auto deepValue = JsonUtils::GetValueByPath<std::string>(data, "a.b.c.d");
    ASSERT_TRUE(deepValue.has_value());
    EXPECT_EQ(*deepValue, "最终值");
    
    // 测试数组元素
    auto arrayValue = JsonUtils::GetValueByPath<int>(data, "a.b.c.e.2");
    ASSERT_TRUE(arrayValue.has_value());
    EXPECT_EQ(*arrayValue, 3);
    
    // 测试带引号的键名
    EXPECT_TRUE(JsonUtils::PathExists(data, R"("x.y.z")"));
    auto dotInKey = JsonUtils::GetValueByPath<std::string>(data, R"("x.y.z")");
    ASSERT_TRUE(dotInKey.has_value());
    EXPECT_EQ(*dotInKey, "点号在键名中");
    
    // 测试不带引号的键名（回退机制）
    auto dotInKeyFallback = JsonUtils::GetValueByPath<std::string>(data, "x.y.z");
    ASSERT_TRUE(dotInKeyFallback.has_value());
    EXPECT_EQ(*dotInKeyFallback, "点号在键名中");
    
    // 测试嵌套带点号的键名 - 使用引号
    auto nestedDot = JsonUtils::GetValueByPath<std::string>(data, R"(a.b.c."f.g.h")");
    ASSERT_TRUE(nestedDot.has_value());
    EXPECT_EQ(*nestedDot, "带点号的键名");
    
    // 测试嵌套带点号的键名 - 不使用引号（回退机制）
    auto nestedDotFallback = JsonUtils::GetValueByPath<std::string>(data, "a.b.c.f.g.h");
    ASSERT_TRUE(nestedDotFallback.has_value());
    EXPECT_EQ(*nestedDotFallback, "带点号的键名");
}

// 测试错误处理
TEST_F(JsonUtilsTest, ErrorHandling) {
    // 无效路径获取
    nlohmann::json json = {{"valid", "value"}};
    auto invalidPath = JsonUtils::GetValueByPath<std::string>(json, "invalid.path");
    EXPECT_FALSE(invalidPath.has_value());
    
    // 类型不匹配
    auto wrongType = JsonUtils::GetValueByPath<int>(json, "valid");
    EXPECT_FALSE(wrongType.has_value());
    
    // 数组越界
    nlohmann::json arrayJson = {{"array", {1,2,3}}};
    auto outOfBounds = JsonUtils::GetValueByPath<int>(arrayJson, "array.5");
    EXPECT_FALSE(outOfBounds.has_value());
    
    // 无效索引
    auto invalidIndex = JsonUtils::GetValueByPath<int>(arrayJson, "array.invalid");
    EXPECT_FALSE(invalidIndex.has_value());
}