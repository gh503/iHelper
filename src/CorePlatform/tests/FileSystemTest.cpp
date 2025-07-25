#include <gtest/gtest.h>
#include "CorePlatform/FileSystem.h"
#include "CorePlatform/Internal/PlatformDetection.h"
#include "TestUtils.h"
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>

namespace CP = CorePlatform;
namespace CPPT = CorePlatformTest;

class FileSystemTest : public ::testing::Test {
protected:
    void SetUp()  {
        std::cout << "Running tests on: " << CPPT::PlatformTestTag::OS << std::endl;
        std::cout << "File system case sensitive: " 
                  << (CPPT::PlatformTestTag::CaseSensitiveFS ? "Yes" : "No") << std::endl;

        tempDir = std::make_unique<CPPT::TempDirectory>("FileSystemTest");
    }

    void TearDown()  {
        // TempDirectory会在析构时自动清理
        tempDir.reset();
    }

    std::string CreateTestFilePath(const std::string& filename) {
        return tempDir->CreateFilePath(filename);
    }

    std::string CreateTestSubDir(const std::string& dirname) {
        return tempDir->CreateSubDirectory(dirname);
    }

    std::unique_ptr<CPPT::TempDirectory> tempDir;
};

// 基本文件操作测试
TEST_F(FileSystemTest, BasicFileOperations) {
    std::string filePath = CreateTestFilePath("testfile.txt");
    std::string content = "Hello, CorePlatform! 测试文件系统: こんにちは, 안녕하세요";
    
    // 1. 文件不存在
    EXPECT_FALSE(CP::FileSystem::Exists(filePath));
    
    // 2. 写入文件
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(filePath, content));
    EXPECT_TRUE(CP::FileSystem::Exists(filePath));
    EXPECT_FALSE(CP::FileSystem::IsDirectory(filePath));
    
    // 3. 读取文件
    std::string readContent = CP::FileSystem::ReadTextFile(filePath);
    EXPECT_EQ(content, readContent);
    
    // 4. 文件大小
    EXPECT_EQ(content.size(), CP::FileSystem::GetFileSize(filePath));
    
    // 5. 删除文件
    EXPECT_TRUE(CP::FileSystem::RemoveFile(filePath));
    EXPECT_FALSE(CP::FileSystem::Exists(filePath));
}

// 二进制文件操作测试
TEST_F(FileSystemTest, BinaryFileOperations) {
    std::string filePath = CreateTestFilePath("data.bin");
    
    // 生成随机二进制数据 (1KB)
    std::vector<uint8_t> testData = CPPT::GenerateRandomData(1024);
    
    // 写入二进制文件
    EXPECT_TRUE(CP::FileSystem::WriteFile(filePath, testData));
    
    // 读取二进制文件
    auto readData = CP::FileSystem::ReadFile(filePath);
    EXPECT_EQ(testData.size(), readData.size());
    EXPECT_TRUE(std::equal(testData.begin(), testData.end(), readData.begin()));
    
    // 追加数据
    std::vector<uint8_t> appendData = {0xAA, 0xBB, 0xCC};
    EXPECT_TRUE(CP::FileSystem::AppendFile(filePath, appendData));
    
    // 验证追加后的数据
    auto fullData = testData;
    fullData.insert(fullData.end(), appendData.begin(), appendData.end());
    readData = CP::FileSystem::ReadFile(filePath);
    EXPECT_EQ(fullData.size(), readData.size());
    EXPECT_TRUE(std::equal(fullData.begin(), fullData.end(), readData.begin()));
}

// 目录操作测试
TEST_F(FileSystemTest, DirectoryOperations) {
    // 1. 创建目录
    std::string dirPath = CreateTestSubDir("testdir");
    EXPECT_TRUE(CP::FileSystem::IsDirectory(dirPath));
    EXPECT_TRUE(CP::FileSystem::Exists(dirPath));
    
    // 2. 在目录中创建文件
    std::string filePath = dirPath + CP_PATH_SEPARATOR_STR + "file.txt";
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(filePath, "Test"));
    EXPECT_TRUE(CP::FileSystem::Exists(filePath));
    
    // 3. 创建多级目录
    std::string deepDir = dirPath + CP_PATH_SEPARATOR_STR + "a" + 
                          CP_PATH_SEPARATOR_STR + "b" + 
                          CP_PATH_SEPARATOR_STR + "c";
    EXPECT_TRUE(CP::FileSystem::NewDirectories(deepDir));
    EXPECT_TRUE(CP::FileSystem::IsDirectory(deepDir));
    
    // 4. 删除非空目录（应失败）
    EXPECT_FALSE(CP::FileSystem::DeleteDirectory(dirPath));
    
    // 5. 删除目录及其内容
    EXPECT_TRUE(CP::FileSystem::DeleteDirectoriesRecursive(dirPath));
    EXPECT_FALSE(CP::FileSystem::Exists(dirPath));
}

// 路径操作测试
TEST_F(FileSystemTest, PathOperations) {
    // 1. 路径拼接
    std::vector<std::string> parts = {tempDir->GetPath(), "dir1", "dir2", "file.txt"};
    std::string fullPath = CP::FileSystem::PathJoin(parts);
    
    #if CP_PLATFORM_WINDOWS
        EXPECT_EQ(fullPath, tempDir->GetPath() + "\\dir1\\dir2\\file.txt");
    #else
        EXPECT_EQ(fullPath, tempDir->GetPath() + "/dir1/dir2/file.txt");
    #endif
    
    // 2. 获取文件名
    EXPECT_EQ("file.txt", CP::FileSystem::GetFileName(fullPath));
    
    // 3. 获取父目录
    std::string parentDir = CP::FileSystem::GetParentDirectory(fullPath);
    #if CP_PLATFORM_WINDOWS
        EXPECT_EQ(parentDir, tempDir->GetPath() + "\\dir1\\dir2");
    #else
        EXPECT_EQ(parentDir, tempDir->GetPath() + "/dir1/dir2");
    #endif
    
    // 4. 获取文件扩展名
    EXPECT_EQ(".txt", CP::FileSystem::GetFileExtension(fullPath));
    
    // 5. 更改文件扩展名
    std::string newPath = CP::FileSystem::ChangeFileExtension(fullPath, ".dat");
    EXPECT_EQ(CP::FileSystem::GetFileExtension(newPath), ".dat");
}

// 文件移动和复制测试
TEST_F(FileSystemTest, MoveAndCopyOperations) {
    std::string sourcePath = CreateTestFilePath("source.txt");
    std::string destPath = CreateTestFilePath("dest.txt");
    std::string copyPath = CreateTestFilePath("copy.txt");
    
    // 1. 创建源文件
    std::string content = "This is a test file for move and copy operations";
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(sourcePath, content));
    
    // 2. 复制文件
    EXPECT_TRUE(CP::FileSystem::DuplicateFile(sourcePath, copyPath));
    EXPECT_TRUE(CP::FileSystem::Exists(sourcePath));
    EXPECT_TRUE(CP::FileSystem::Exists(copyPath));
    EXPECT_EQ(CP::FileSystem::ReadTextFile(copyPath), content);
    
    // 3. 移动文件
    EXPECT_TRUE(CP::FileSystem::RelocateFile(sourcePath, destPath));
    EXPECT_FALSE(CP::FileSystem::Exists(sourcePath));
    EXPECT_TRUE(CP::FileSystem::Exists(destPath));
    EXPECT_EQ(CP::FileSystem::ReadTextFile(destPath), content);
    
    // 4. 覆盖复制
    std::string newContent = "New content for overwrite test";
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(sourcePath, newContent));
    EXPECT_TRUE(CP::FileSystem::DuplicateFile(sourcePath, destPath, true));
    EXPECT_EQ(CP::FileSystem::ReadTextFile(destPath), newContent);
    
    // 5. 尝试覆盖而不允许覆盖（应失败）
    EXPECT_FALSE(CP::FileSystem::DuplicateFile(sourcePath, destPath, false));
}

// 目录遍历测试
TEST_F(FileSystemTest, DirectoryEnumeration) {
    // 创建测试目录结构
    std::string baseDir = CreateTestSubDir("enum_test");
    std::vector<std::string> files = {
        baseDir + CP_PATH_SEPARATOR_STR + "file1.txt",
        baseDir + CP_PATH_SEPARATOR_STR + "file2.dat",
        baseDir + CP_PATH_SEPARATOR_STR + "image.png"
    };
    
    std::vector<std::string> dirs = {
        baseDir + CP_PATH_SEPARATOR_STR + "docs",
        baseDir + CP_PATH_SEPARATOR_STR + "images",
        baseDir + CP_PATH_SEPARATOR_STR + "temp"
    };
    
    // 创建文件和目录
    for (const auto& file : files) {
        EXPECT_TRUE(CP::FileSystem::WriteTextFile(file, "content"));
    }
    
    for (const auto& dir : dirs) {
        EXPECT_TRUE(CP::FileSystem::NewDirectory(dir));
    }
    
    // 枚举目录内容
    std::vector<std::string> entries;
    EXPECT_TRUE(CP::FileSystem::ListDirectory(baseDir, entries));
    
    // 验证结果（排序以便比较）
    std::sort(entries.begin(), entries.end());
    std::vector<std::string> expected;
    for (const auto& dir : dirs) {
        expected.push_back(CP::FileSystem::GetFileName(dir));
    }
    for (const auto& file : files) {
        expected.push_back(CP::FileSystem::GetFileName(file));
    }
    std::sort(expected.begin(), expected.end());
    
    EXPECT_EQ(entries.size(), expected.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        EXPECT_EQ(entries[i], expected[i]);
    }
    
    // 递归枚举
    std::vector<std::string> allEntries;
    EXPECT_TRUE(CP::FileSystem::ListDirectoryRecursive(baseDir, allEntries));
    
    // 应包含所有文件和目录
    EXPECT_EQ(allEntries.size(), files.size() + dirs.size());
}

// 特殊文件类型测试
TEST_F(FileSystemTest, SpecialFileTypes) {
    std::string filePath = CreateTestFilePath("normal.txt");
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(filePath, "test"));
    
    // 1. 普通文件
    EXPECT_TRUE(CP::FileSystem::IsRegularFile(filePath));
    EXPECT_FALSE(CP::FileSystem::IsDirectory(filePath));
    EXPECT_FALSE(CP::FileSystem::IsSymlink(filePath));
    
    // 2. 符号链接（仅支持类Unix系统）
    #if !CP_PLATFORM_WINDOWS
        std::string linkPath = CreateTestFilePath("link.txt");
        EXPECT_TRUE(CP::FileSystem::CreateSymlink(filePath, linkPath));
        EXPECT_TRUE(CP::FileSystem::IsSymlink(linkPath));
        
        // 读取链接目标
        std::string target = CP::FileSystem::ReadSymlink(linkPath);
        EXPECT_EQ(target, filePath);
        
        // 通过链接访问文件
        EXPECT_EQ(CP::FileSystem::ReadTextFile(linkPath), "test");
    #endif
}

// 跨平台行为测试
TEST_F(FileSystemTest, CrossPlatformBehavior) {
    using Tag = CPPT::PlatformTestTag;
    
    // 1. 大小写敏感性测试
    std::string filePath = CreateTestFilePath("CaseTest.txt");
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(filePath, "Case sensitivity test"));
    
    std::string upperPath = CreateTestFilePath("CASETEST.TXT");
    if (Tag::CaseSensitiveFS) {
        // Linux: 大小写敏感
        EXPECT_FALSE(CP::FileSystem::Exists(upperPath));
    } else {
        // Windows和macOS: 大小写不敏感
        EXPECT_TRUE(CP::FileSystem::Exists(upperPath));
    }
    
    // 2. 路径分隔符测试
    std::string mixedPath = tempDir->GetPath() + "/mixed" + CP_PATH_SEPARATOR_STR + "path\\file.txt";
    #if CP_PLATFORM_WINDOWS
        // Windows应能处理混合分隔符
        EXPECT_TRUE(CP::FileSystem::NewDirectories(mixedPath));
    #else
        // 其他平台应正常处理
        EXPECT_TRUE(CP::FileSystem::NewDirectories(mixedPath));
    #endif
    
    // 3. 特殊字符测试
    std::string specialPath = CreateTestFilePath("file with spaces and @#$%^&()[]{}.txt");
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(specialPath, "Special characters test"));
    EXPECT_TRUE(CP::FileSystem::Exists(specialPath));
}

// Unicode和多语言支持测试
TEST_F(FileSystemTest, UnicodeSupport) {
    // 生成包含多种Unicode字符的文件名
    std::string fileName = "测试文件_" + CPPT::GenerateRandomString(5) + "_日本語_한글.txt";
    std::string filePath = CreateTestFilePath(fileName);
    
    // 写入包含Unicode的内容
    std::string content = "Unicode测试: こんにちは, 안녕하세요, привет, γεια σας, مرحبا";
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(filePath, content));
    
    // 验证文件存在
    EXPECT_TRUE(CP::FileSystem::Exists(filePath));
    
    // 验证文件内容
    std::string readContent = CP::FileSystem::ReadTextFile(filePath);
    EXPECT_EQ(content, readContent);
    
    // 在目录中使用Unicode
    std::string unicodeDir = CreateTestSubDir("目录_文件夹_DIR");
    std::string unicodeFilePath = unicodeDir + CP_PATH_SEPARATOR_STR + "文件.txt";
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(unicodeFilePath, "测试"));
    EXPECT_TRUE(CP::FileSystem::Exists(unicodeFilePath));
}

// 大文件支持测试
TEST_F(FileSystemTest, LargeFileSupport) {
    // 创建大文件 (10MB)
    std::string filePath = CreateTestFilePath("largefile.bin");
    std::vector<uint8_t> largeData = CPPT::GenerateRandomData(10 * 1024 * 1024); // 10MB
    
    // 写入文件
    auto start = std::chrono::high_resolution_clock::now();
    EXPECT_TRUE(CP::FileSystem::WriteFile(filePath, largeData));
    auto end = std::chrono::high_resolution_clock::now();
    
    // 输出写入性能
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    double speed = (10.0 * 1000) / duration; // MB/s
    std::cout << "Large file write: 10MB in " << duration << "ms (" << speed << " MB/s)" << std::endl;
    
    // 验证文件大小
    EXPECT_EQ(largeData.size(), CP::FileSystem::GetFileSize(filePath));
    
    // 读取文件
    start = std::chrono::high_resolution_clock::now();
    auto readData = CP::FileSystem::ReadFile(filePath);
    end = std::chrono::high_resolution_clock::now();
    
    // 输出读取性能
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    speed = (10.0 * 1000) / duration; // MB/s
    std::cout << "Large file read: 10MB in " << duration << "ms (" << speed << " MB/s)" << std::endl;
    
    // 验证内容
    EXPECT_EQ(largeData.size(), readData.size());
    EXPECT_TRUE(std::equal(largeData.begin(), largeData.end(), readData.begin()));
}

// 当前工作目录测试
TEST_F(FileSystemTest, CurrentWorkingDirectory) {
    // 保存原始工作目录
    std::string originalDir = CP::FileSystem::GetCurDirectory();
    EXPECT_FALSE(originalDir.empty());
    
    // 更改为临时目录
    EXPECT_TRUE(CP::FileSystem::SetCurDirectory(tempDir->GetPath()));
    EXPECT_EQ(CP::FileSystem::GetCurDirectory(), tempDir->GetPath());
    
    // 在临时目录中创建文件
    std::string filePath = "current_dir_test.txt";
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(filePath, "Test"));
    EXPECT_TRUE(CP::FileSystem::Exists(filePath));
    
    // 恢复原始工作目录
    EXPECT_TRUE(CP::FileSystem::SetCurDirectory(originalDir));
    EXPECT_EQ(CP::FileSystem::GetCurDirectory(), originalDir);
}

// 临时目录功能测试
TEST_F(FileSystemTest, TempDirectoryFunction) {
    std::string tempDirPath = CP::FileSystem::GetTempDirectory();
    EXPECT_FALSE(tempDirPath.empty());
    EXPECT_TRUE(CP::FileSystem::IsDirectory(tempDirPath));
    
    // 在临时目录中创建文件
    std::string tempFilePath = tempDirPath + CP_PATH_SEPARATOR_STR + "tempfile.tmp";
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(tempFilePath, "Temporary content"));
    EXPECT_TRUE(CP::FileSystem::Exists(tempFilePath));
    
    // 清理
    EXPECT_TRUE(CP::FileSystem::RemoveFile(tempFilePath));
}

// 可执行文件路径测试
TEST_F(FileSystemTest, ExecutablePath) {
    std::string exePath = CP::FileSystem::GetExecutablePath();
    EXPECT_FALSE(exePath.empty());
    EXPECT_TRUE(CP::FileSystem::Exists(exePath));
    
    #if CP_PLATFORM_WINDOWS
        EXPECT_TRUE(CP::FileSystem::GetFileExtension(exePath) == ".exe");
    #endif
    
    // 获取可执行文件所在目录
    std::string exeDir = CP::FileSystem::GetParentDirectory(exePath);
    EXPECT_TRUE(CP::FileSystem::IsDirectory(exeDir));
}

// 文件时间戳测试
TEST_F(FileSystemTest, FileTimestamps) {
    std::string filePath = CreateTestFilePath("timestamp.txt");
    
    // 获取创建前的时间
    auto beforeCreation = std::chrono::system_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 创建文件
    EXPECT_TRUE(CP::FileSystem::WriteTextFile(filePath, "Timestamp test"));
    
    // 获取创建后的时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto afterCreation = std::chrono::system_clock::now();
    
    // 获取文件时间戳
    auto createTime = CP::FileSystem::GetCreationTime(filePath);
    auto modifyTime = CP::FileSystem::GetModificationTime(filePath);
    
    // 验证时间戳在合理范围内
    EXPECT_TRUE(createTime >= beforeCreation);
    EXPECT_TRUE(createTime <= afterCreation);
    EXPECT_TRUE(modifyTime >= beforeCreation);
    EXPECT_TRUE(modifyTime <= afterCreation);
    
    // 修改文件时间戳
    auto newTime = beforeCreation - std::chrono::hours(24); // 设置为24小时前
    EXPECT_TRUE(CP::FileSystem::SetModificationTime(filePath, newTime));
    
    // 验证修改后的时间戳
    auto updatedTime = CP::FileSystem::GetModificationTime(filePath);
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(updatedTime - newTime);
    EXPECT_LT(std::abs(diff.count()), 2); // 允许2秒误差
}