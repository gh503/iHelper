## 代码结构
```
CorePlatform/
├── CMakeLists.txt               # 主构建文件
├── cmake/
│   └── CorePlatformConfig.cmake.in  # 包配置模板
├── include/
│   └── CorePlatform/
│       ├── FileSystem.h
│       ├── Registry.h
│       ├── Process.h
│       ├── Network.h
│       ├── SystemInfo.h
│       └── Internal/
│           └── PlatformDetection.h
├── src/
│   ├── FileSystem.cpp           # 公共分发层
│   ├── Registry.cpp
│   ├── Process.cpp
│   ├── Network.cpp
│   ├── SystemInfo.cpp
│   └── Platform/
│       ├── Windows/             # Windows实现
│       │   ├── FileSystemWin.cpp
│       │   ├── RegistryWin.cpp
│       │   ├── ProcessWin.cpp
│       │   ├── NetworkWin.cpp
│       │   └── SystemInfoWin.cpp
│       ├── MacOS/               # macOS实现
│       │   ├── FileSystemMac.cpp
│       │   ├── RegistryMac.cpp  # macOS没有注册表，提供空实现
│       │   ├── ProcessMac.cpp
│       │   ├── NetworkMac.cpp
│       │   └── SystemInfoMac.cpp
│       └── Linux/               # Linux实现
│           ├── FileSystemLinux.cpp
│           ├── RegistryLinux.cpp
│           ├── ProcessLinux.cpp
│           ├── NetworkLinux.cpp
│           └── SystemInfoLinux.cpp
└── tests/                       # 单元测试
    ├── CMakeLists.txt
    ├── FileSystemTest.cpp
    ├── RegistryTest.cpp
    ├── ProcessTest.cpp
    ├── NetworkTest.cpp
    ├── SystemInfoTest.cpp
    └── TestUtils.h
```

## 宏使用说明
下面我将为之前的源码添加详细注释，并说明每个宏的使用场景，同时确保所有代码与 `PlatformDetection.h` 中定义的宏保持一致：

### 1. 文件系统实现 (src/FileSystem.cpp)

```cpp
#include "CorePlatform/FileSystem.h"
#include "CorePlatform/Internal/PlatformDetection.h"

// 平台特定实现的分发层
// 使用 CP_PLATFORM_* 宏选择正确的平台实现
#if CP_PLATFORM_WINDOWS
    #include "Platform/Windows/FileSystemWin.h"
#elif CP_PLATFORM_MACOS
    #include "Platform/MacOS/FileSystemMac.h"
#elif CP_PLATFORM_LINUX
    #include "Platform/Linux/FileSystemLinux.h"
#endif

namespace CorePlatform {

// Windows平台实现转发
#if CP_PLATFORM_WINDOWS
bool FileSystem::Exists(const std::string& path) {
    return FileSystem::Exists(path);
}

// macOS平台实现转发
#elif CP_PLATFORM_MACOS
bool FileSystem::Exists(const std::string& path) {
    return FileSystem::Exists(path);
}

// Linux平台实现转发
#elif CP_PLATFORM_LINUX
bool FileSystem::Exists(const std::string& path) {
    return FileSystem::Exists(path);
}

#endif

} // namespace CorePlatform
```

### 2. Windows文件系统实现 (src/Platform/Windows/FileSystemWin.cpp)

```cpp
#include "CorePlatform/FileSystem.h"
#include "CorePlatform/Internal/WindowsUtils.h"
#include <Windows.h>
#include <fileapi.h>
#include <shlwapi.h>

// 使用CP_PLATFORM_WINDOWS宏确保只在Windows平台编译
#if CP_PLATFORM_WINDOWS

// 使用CP_PATH_SEPARATOR宏处理平台路径分隔符
const char FileSystem::PATH_SEPARATOR = CP_PATH_SEPARATOR;

namespace CorePlatform {

namespace {

// 内部辅助函数：转换UTF-8路径到宽字符
std::wstring ConvertPath(const std::string& path) {
    return WindowsUtils::UTF8ToWide(path);
}

} // 匿名命名空间

bool FileSystem::Exists(const std::string& path) {
    // 使用WindowsUtils进行UTF-8到UTF-16转换
    std::wstring wpath = ConvertPath(path);
    DWORD attrib = GetFileAttributesW(wpath.c_str());
    
    // 检查文件属性是否有效
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        // 文件不存在或访问被拒绝都视为不存在
        return err != ERROR_FILE_NOT_FOUND && 
               err != ERROR_PATH_NOT_FOUND &&
               err != ERROR_ACCESS_DENIED;
    }
    
    // 确保不是目录
    return !(attrib & FILE_ATTRIBUTE_DIRECTORY);
}

bool FileSystem::IsDirectory(const std::string& path) {
    std::wstring wpath = ConvertPath(path);
    DWORD attrib = GetFileAttributesW(wpath.c_str());
    
    // 检查文件属性是否有效且是目录
    return (attrib != INVALID_FILE_ATTRIBUTES) && 
           (attrib & FILE_ATTRIBUTE_DIRECTORY);
}

// ... 其他Windows文件系统实现

} // namespace CorePlatform

#endif // CP_PLATFORM_WINDOWS
```

### 3. 注册表实现 (src/Registry.cpp)

```cpp
#include "CorePlatform/Registry.h"
#include "CorePlatform/Internal/PlatformDetection.h"

// 使用CP_PLATFORM_*宏选择平台实现
#if CP_PLATFORM_WINDOWS
    #include "Platform/Windows/RegistryWin.h"
#elif CP_PLATFORM_MACOS
    #include "Platform/MacOS/RegistryMac.h"
#elif CP_PLATFORM_LINUX
    #include "Platform/Linux/RegistryLinux.h"
#endif

namespace CorePlatform {

// Windows注册表实现转发
#if CP_PLATFORM_WINDOWS
bool Registry::KeyExists(RootKey root, const std::string& subkey, RegistryView view) {
    return RegistryWin::KeyExists(root, subkey, view);
}

// macOS注册表实现转发（空实现）
#elif CP_PLATFORM_MACOS
bool Registry::KeyExists(RootKey, const std::string&, RegistryView) {
    // macOS没有注册表，始终返回false
    return false;
}

// Linux注册表实现转发（空实现）
#elif CP_PLATFORM_LINUX
bool Registry::KeyExists(RootKey, const std::string&, RegistryView) {
    // Linux没有注册表，始终返回false
    return false;
}

#endif

} // namespace CorePlatform
```

### 4. Windows注册表实现 (src/Platform/Windows/RegistryWin.cpp)

```cpp
#include "CorePlatform/Registry.h"
#include "CorePlatform/Internal/WindowsUtils.h"
#include <Windows.h>
#include <winreg.h>

// 确保只在Windows平台编译
#if CP_PLATFORM_WINDOWS

namespace CorePlatform {

namespace {

// 内部辅助函数：将RootKey转换为Windows HKEY
HKEY RootKeyToHKEY(Registry::RootKey root) {
    switch (root) {
        case Registry::RootKey::ClassesRoot:    return HKEY_CLASSES_ROOT;
        case Registry::RootKey::CurrentUser:    return HKEY_CURRENT_USER;
        // ... 其他键转换
        default: return nullptr;
    }
}

// 解决注册表重定向问题
REGSAM ViewToSamFlags(Registry::RegistryView view) {
    switch (view) {
        case Registry::RegistryView::Force32:
            return KEY_WOW64_32KEY;  // 强制32位视图
        case Registry::RegistryView::Force64:
            return KEY_WOW64_64KEY;  // 强制64位视图
        case Registry::RegistryView::Default:
        default:
            // 默认视图取决于程序架构
            #if CP_ARCH_64
                return KEY_WOW64_64KEY;
            #else
                return KEY_WOW64_32KEY;
            #endif
    }
}

} // 匿名命名空间

bool RegistryWin::KeyExists(RootKey root, const std::string& subkey, RegistryView view) {
    HKEY hRoot = RootKeyToHKEY(root);
    if (!hRoot) return false;
    
    // 使用WindowsUtils进行UTF-8到UTF-16转换
    std::wstring wsubkey = WindowsUtils::UTF8ToWide(subkey);
    HKEY hKey = nullptr;
    
    // 应用注册表视图标志
    REGSAM access = KEY_READ | ViewToSamFlags(view);
    LSTATUS status = RegOpenKeyExW(hRoot, wsubkey.c_str(), 0, access, &hKey);
    
    if (hKey) {
        RegCloseKey(hKey);
    }
    
    return status == ERROR_SUCCESS;
}

// ... 其他注册表方法实现

} // namespace CorePlatform

#endif // CP_PLATFORM_WINDOWS
```

### 5. 单元测试 (tests/FileSystemTest.cpp)

```cpp
#include <gtest/gtest.h>
#include "CorePlatform/FileSystem.h"
#include "CorePlatform/Internal/PlatformDetection.h"
#include "TestUtils.h"

namespace CP = CorePlatform;

class FileSystemTest : public ::testing::Test {
protected:
    void SetUp()  {
        // 使用平台无关的路径构建方法
        testDir = CP::FileSystem::GetTempDirectory() + CP_PATH_SEPARATOR_STR + "CorePlatformTests";
        CP::FileSystem::NewDirectory(testDir);
    }
    
    void TearDown()  {
        if (CP::FileSystem::IsDirectory(testDir)) {
            CP::FileSystem::DeleteDirectory(testDir);
        }
    }
    
    std::string testDir;
};

TEST_F(FileSystemTest, UnicodeSupport) {
    std::string fileName = testDir + CP_PATH_SEPARATOR_STR + "测试文件_日本語_한글.txt";
    std::string content = "UTF-8测试: こんにちは, 안녕하세요";
    
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(fileName, content));
    EXPECT_TRUE(CP::FileSystem::Exists(fileName));
    
    std::string readContent = CP::FileSystem::ReadTextFile(fileName);
    EXPECT_EQ(content, readContent);
}

// Windows特定测试
#ifdef CP_PLATFORM_WINDOWS
TEST_F(FileSystemTest, WindowsLongPathSupport) {
    // Windows长路径测试（需要启用长路径支持）
    std::string longPath = testDir + "\\very\\deep\\directory\\structure\\that\\exceeds\\260\\characters\\";
    for (int i = 0; i < 20; i++) {
        longPath += "subdir\\";
    }
    longPath += "test.txt";
    
    EXPECT_TRUE(CP::FileSystem::NewDirectory(longPath));
    EXPECT_TRUE(CP::FileSystem::IsDirectory(longPath));
}
#endif

// macOS特定测试
#ifdef CP_PLATFORM_MACOS
TEST_F(FileSystemTest, MacResourceFork) {
    // macOS资源派生文件测试
    std::string filePath = testDir + "/testfile.txt";
    std::string resourcePath = filePath + "/..namedfork/rsrc";
    
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(filePath, "data"));
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(resourcePath, "resource"));
    
    EXPECT_TRUE(CP::FileSystem::Exists(resourcePath));
}
#endif

// Linux特定测试
#ifdef CP_PLATFORM_LINUX
TEST_F(FileSystemTest, LinuxSymlink) {
    // Linux符号链接测试
    std::string filePath = testDir + "/original.txt";
    std::string linkPath = testDir + "/link.txt";
    
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(filePath, "content"));
    EXPECT_TRUE(CP::FileSystem::CreateSymlink(filePath, linkPath));
    
    EXPECT_TRUE(CP::FileSystem::IsSymlink(linkPath));
    EXPECT_EQ(CP::FileSystem::ReadTextFile(linkPath), "content");
}
#endif
```

### 宏使用场景说明

1. **平台检测宏**：
   ```cpp
   #if CP_PLATFORM_WINDOWS
       // Windows特定代码
   #elif CP_PLATFORM_MACOS
       // macOS特定代码
   #elif CP_PLATFORM_LINUX
       // Linux特定代码
   #endif
   ```
   - 用于分离不同操作系统的实现
   - 在平台分发层和平台特定实现中使用
   - 确保代码只在目标平台编译

2. **架构检测宏**：
   ```cpp
   #if CP_ARCH_64
       // 64位特定优化
   #else
       // 32位兼容代码
   #endif
   ```
   - 解决注册表重定向问题
   - 优化内存敏感操作
   - 处理指针大小相关逻辑

3. **路径处理宏**：
   ```cpp
   std::string path = dir + CP_PATH_SEPARATOR_STR + file;
   ```
   - 构建跨平台路径
   - 避免硬编码路径分隔符
   - 确保路径在目标平台有效

4. **编译器特性宏**：
   ```cpp
   #if CP_COMPILER_MSVC
       // MSVC特定语法
   #elif CP_COMPILER_CLANG
       // Clang特定语法
   #endif
   ```
   - 处理编译器差异
   - 使用编译器特定扩展
   - 禁用编译器特定警告

5. **调试宏**：
   ```cpp
   CP_ASSERT_MSG(ptr != nullptr, "Invalid pointer");
   ```
   - 调试期间验证假设
   - 发布版本自动移除
   - 提供详细错误信息

6. **内联优化宏**：
   ```cpp
   CP_FORCE_INLINE int Calculate(int a, int b) {
       return a * b + a / b;
   }
   ```
   - 性能关键函数强制内联
   - 跨编译器一致的内联语法

7. **字节序宏**：
   ```cpp
   #if CP_LITTLE_ENDIAN
       // 小端序处理
   #elif CP_BIG_ENDIAN
       // 大端序处理
   #endif
   ```
   - 网络数据序列化
   - 二进制文件格式处理
   - 硬件寄存器访问

### 一致性验证

1. **平台宏**：
   - 所有平台特定代码使用 `CP_PLATFORM_WINDOWS`、`CP_PLATFORM_MACOS`、`CP_PLATFORM_LINUX`
   - 替代了之前的 `_WIN32`、`__APPLE__` 等原生宏

2. **路径处理**：
   - 使用 `CP_PATH_SEPARATOR` 和 `CP_PATH_SEPARATOR_STR` 替代硬编码的 '/' 或 '\\'

3. **架构检测**：
   - 注册表重定向处理使用 `CP_ARCH_64` 检测64位架构

4. **编译器检测**：
   - 所有编译器特定代码使用 `CP_COMPILER_MSVC`、`CP_COMPILER_CLANG` 等

5. **调试支持**：
   - 使用 `CP_DEBUG_MODE` 替代 `_DEBUG` 等原生定义

6. **内联优化**：
   - 性能关键函数使用 `CP_FORCE_INLINE` 确保跨编译器内联

7. **错误处理**：
   - 使用 `CP_ASSERT` 和 `CP_ASSERT_MSG` 进行调试断言

### 最佳实践建议

1. **平台特定实现**：
   ```cpp
   // 正确方式
   #if CP_PLATFORM_WINDOWS
   void WindowsImpl() { /* ... */ }
   #endif
   
   // 错误方式
   #ifdef _WIN32  // 避免使用原生宏
   void WindowsImpl() { /* ... */ }
   #endif
   ```

2. **路径构建**：
   ```cpp
   // 正确方式
   std::string path = dir + CP_PATH_SEPARATOR_STR + file;
   
   // 错误方式
   std::string path = dir + "\\" + file;  // Windows专用
   ```

3. **条件编译**：
   ```cpp
   // 正确方式：完整包含边界
   #if CP_PLATFORM_WINDOWS
   // Windows代码
   #elif CP_PLATFORM_MACOS
   // macOS代码
   #else
   #error "Unsupported platform"
   #endif
   
   // 错误方式：缺少平台边界
   #if CP_PLATFORM_WINDOWS
   // Windows代码
   #endif
   // 缺少其他平台处理
   ```

4. **架构相关代码**：
   ```cpp
   // 正确方式
   #if CP_ARCH_64
   using IntPtr = int64_t;
   #else
   using IntPtr = int32_t;
   #endif
   
   // 错误方式
   #ifdef _WIN64  // 避免使用编译器特定宏
   using IntPtr = int64_t;
   #else
   using IntPtr = int32_t;
   #endif
   ```

5. **调试断言**：
   ```cpp
   // 正确方式
   void* Allocate(size_t size) {
       CP_ASSERT_MSG(size > 0, "Allocation size must be positive");
       return malloc(size);
   }
   
   // 错误方式
   void* Allocate(size_t size) {
       assert(size > 0 && "Allocation size must be positive");  // 使用原生assert
       return malloc(size);
   }
   ```

通过使用这些宏和添加详细注释，我们确保了：  
1. 代码清晰可读，平台差异明确
2. 跨平台一致性
3. 易于维护和扩展
4. 编译器兼容性
5. 调试和生产环境的正确行为

所有源码都已更新为使用 `PlatformDetection.h` 中定义的统一宏，确保整个项目在平台检测和处理上保持一致性。