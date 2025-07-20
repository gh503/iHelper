#include <gtest/gtest.h>
#include "CorePlatform/Windows/Registry.h"
#include "TestUtils.h"

namespace CP = CorePlatform;

class RegistryTest : public ::testing::Test {
protected:
    void SetUp()  {
        testKey = "Software\\CorePlatformTests";
        #ifdef CP_PLATFORM_WINDOWS
            // 确保测试键存在
            CP::Registry::CreateKey(CP::Registry::RootKey::CurrentUser, testKey);
        #endif
    }
    
    void TearDown()  {
        #ifdef CP_PLATFORM_WINDOWS
            // 清理测试键
            CP::Registry::DeleteKey(CP::Registry::RootKey::CurrentUser, testKey);
        #endif
    }
    
    std::string testKey;
};

#ifdef CP_PLATFORM_WINDOWS

TEST_F(RegistryTest, RegistryViewOperations) {
    const std::string valueName = "TestView";
    const uint32_t testValue = 0x12345678;
    
    // 在64位视图写入值
    EXPECT_TRUE(CP::Registry::WriteDWord(
        CP::Registry::RootKey::CurrentUser, 
        testKey, 
        valueName, 
        testValue,
        CP::Registry::RegistryView::Force64
    ));
    
    // 在32位视图检查值不存在
    uint32_t readValue = 0;
    EXPECT_FALSE(CP::Registry::ReadDWord(
        CP::Registry::RootKey::CurrentUser, 
        testKey, 
        valueName, 
        readValue,
        CP::Registry::RegistryView::Force32
    ));
    
    // 在64位视图检查值存在
    EXPECT_TRUE(CP::Registry::ReadDWord(
        CP::Registry::RootKey::CurrentUser, 
        testKey, 
        valueName, 
        readValue,
        CP::Registry::RegistryView::Force64
    ));
    EXPECT_EQ(testValue, readValue);
    
    // 在默认视图检查值（取决于程序位数）
    #ifdef _WIN64
        EXPECT_TRUE(CP::Registry::ReadDWord(
            CP::Registry::RootKey::CurrentUser, 
            testKey, 
            valueName, 
            readValue
        ));
    #else
        EXPECT_FALSE(CP::Registry::ReadDWord(
            CP::Registry::RootKey::CurrentUser, 
            testKey, 
            valueName, 
            readValue
        ));
    #endif
    
    // 清理
    EXPECT_TRUE(CP::Registry::DeleteValue(
        CP::Registry::RootKey::CurrentUser, 
        testKey, 
        valueName,
        CP::Registry::RegistryView::Force64
    ));
}

TEST_F(RegistryTest, KeyOperationsWithView) {
    std::string subkey = testKey + "\\SubKey64";
    
    // 在64位视图创建键
    EXPECT_TRUE(CP::Registry::CreateKey(
        CP::Registry::RootKey::CurrentUser, 
        subkey,
        CP::Registry::RegistryView::Force64
    ));
    
    // 检查64位视图存在
    EXPECT_TRUE(CP::Registry::KeyExists(
        CP::Registry::RootKey::CurrentUser, 
        subkey,
        CP::Registry::RegistryView::Force64
    ));
    
    // 检查32位视图不存在
    EXPECT_FALSE(CP::Registry::KeyExists(
        CP::Registry::RootKey::CurrentUser, 
        subkey,
        CP::Registry::RegistryView::Force32
    ));
    
    // 删除64位视图的键
    EXPECT_TRUE(CP::Registry::DeleteKey(
        CP::Registry::RootKey::CurrentUser, 
        subkey,
        CP::Registry::RegistryView::Force64
    ));
}

TEST_F(RegistryTest, CrossViewOperations) {
    std::string subkey32 = testKey + "\\SubKey32";
    std::string subkey64 = testKey + "\\SubKey64";
    
    // 创建32位和64位视图的键
    EXPECT_TRUE(CP::Registry::CreateKey(
        CP::Registry::RootKey::CurrentUser, 
        subkey32,
        CP::Registry::RegistryView::Force32
    ));
    
    EXPECT_TRUE(CP::Registry::CreateKey(
        CP::Registry::RootKey::CurrentUser, 
        subkey64,
        CP::Registry::RegistryView::Force64
    ));
    
    // 检查各自视图存在
    EXPECT_TRUE(CP::Registry::KeyExists(
        CP::Registry::RootKey::CurrentUser, 
        subkey32,
        CP::Registry::RegistryView::Force32
    ));
    
    EXPECT_TRUE(CP::Registry::KeyExists(
        CP::Registry::RootKey::CurrentUser, 
        subkey64,
        CP::Registry::RegistryView::Force64
    ));
    
    // 清理
    EXPECT_TRUE(CP::Registry::DeleteKey(
        CP::Registry::RootKey::CurrentUser, 
        subkey32,
        CP::Registry::RegistryView::Force32
    ));
    
    EXPECT_TRUE(CP::Registry::DeleteKey(
        CP::Registry::RootKey::CurrentUser, 
        subkey64,
        CP::Registry::RegistryView::Force64
    ));
}

#endif // CP_PLATFORM_WINDOWS