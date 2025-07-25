#pragma once

#include "CorePlatform/FileSystem.h"
#include "CorePlatform/Internal/PlatformDetection.h"
#include "CorePlatform/Logger.h"
#include <string>
#include <random>
#include <stdexcept>

namespace CorePlatformTest {

/**
 * @brief 临时目录类 - 在构造时创建，析构时自动删除
 * 
 * 这个类在单元测试中非常有用，可以确保测试结束后清理所有临时文件。
 * 它使用随机生成的目录名来避免冲突。
 */
class TempDirectory {
public:
    /**
     * @brief 构造函数，创建临时目录
     * 
     * @param prefix 临时目录名前缀（可选）
     * 
     * 示例:
     *   TempDirectory temp("MyTest");
     *   std::string filePath = temp.GetPath() + "/testfile.txt";
     */
    explicit TempDirectory(const std::string& prefix = "CorePlatformTest") {
        // 获取系统临时目录
        std::string tempBase = CorePlatform::FileSystem::GetTempDirectory();
        
        // 生成随机目录名
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        
        path = tempBase + CP_PATH_SEPARATOR_STR + 
               prefix + "_" + std::to_string(dis(gen));
        
        // 创建目录
        if (!CorePlatform::FileSystem::NewDirectory(path)) {
            throw std::runtime_error("Failed to create temporary directory: " + path);
        }
    }

    /**
     * @brief 析构函数，删除临时目录及其内容
     */
    ~TempDirectory() {
        if (!path.empty()) {
            // 安全删除目录，忽略错误
            try {
                if (CorePlatform::FileSystem::IsDirectory(path)) {
                    CorePlatform::FileSystem::DeleteDirectory(path);
                }
            } catch (...) {
                // 析构函数中不能抛出异常
            }
        }
    }

    // 禁用拷贝和移动
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    TempDirectory(TempDirectory&&) = delete;
    TempDirectory& operator=(TempDirectory&&) = delete;

    /**
     * @brief 获取临时目录完整路径
     * 
     * @return std::string 目录路径
     */
    std::string GetPath() const { return path; }

    /**
     * @brief 在临时目录中创建文件路径
     * 
     * @param filename 文件名
     * @return std::string 完整文件路径
     * 
     * 示例:
     *   std::string file = temp.CreateFilePath("data.bin");
     */
    std::string CreateFilePath(const std::string& filename) const {
        return path + CP_PATH_SEPARATOR_STR + filename;
    }

    /**
     * @brief 创建子目录
     * 
     * @param subdir 子目录名
     * @return std::string 子目录完整路径
     */
    std::string CreateSubDirectory(const std::string& subdir) const {
        std::string fullPath = path + CP_PATH_SEPARATOR_STR + subdir;
        if (!CorePlatform::FileSystem::NewDirectory(fullPath)) {
            throw std::runtime_error("Failed to create subdirectory: " + fullPath);
        }
        return fullPath;
    }

private:
    std::string path; // 临时目录路径
};

/**
 * @brief 临时文件类 - 在构造时创建，析构时自动删除
 * 
 * 用于管理测试中的临时文件，确保测试结束后文件被删除。
 */
class TempFile {
public:
    /**
     * @brief 构造函数，创建临时文件
     * 
     * @param extension 文件扩展名（可选）
     * 
     * 示例:
     *   TempFile temp(".txt");
     */
    explicit TempFile(const std::string& extension = "") {
        // 创建临时目录（文件将存放在这里）
        static TempDirectory tempDir("TempFiles");
        
        // 生成随机文件名
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        
        path = tempDir.CreateFilePath("tempfile_" + std::to_string(dis(gen)) + extension);
        
        // 创建空文件
        if (!CorePlatform::FileSystem::WriteFile(path, {})) {
            throw std::runtime_error("Failed to create temporary file: " + path);
        }
    }

    /**
     * @brief 析构函数，删除临时文件
     */
    ~TempFile() {
        if (!path.empty()) {
            // 安全删除文件，忽略错误
            try {
                if (CorePlatform::FileSystem::Exists(path)) {
                    CorePlatform::FileSystem::RemoveFile(path);
                }
            } catch (...) {
                // 析构函数中不能抛出异常
            }
        }
    }

    // 禁用拷贝和移动
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    TempFile(TempFile&&) = delete;
    TempFile& operator=(TempFile&&) = delete;

    /**
     * @brief 获取临时文件完整路径
     * 
     * @return std::string 文件路径
     */
    std::string GetPath() const { return path; }

    /**
     * @brief 向文件写入内容
     * 
     * @param content 要写入的内容
     */
    void WriteContent(const std::string& content) {
        std::vector<uint8_t> data(content.begin(), content.end());
        if (!CorePlatform::FileSystem::WriteFile(path, data)) {
            throw std::runtime_error("Failed to write to temporary file: " + path);
        }
    }

    /**
     * @brief 向文件写入二进制内容
     * 
     * @param data 二进制数据
     */
    void WriteBinary(const std::vector<uint8_t>& data) {
        if (!CorePlatform::FileSystem::WriteFile(path, data)) {
            throw std::runtime_error("Failed to write binary to temporary file: " + path);
        }
    }

    /**
     * @brief 读取文件内容
     * 
     * @return std::string 文件内容
     */
    std::string ReadContent() const {
        auto data = CorePlatform::FileSystem::ReadFile(path);
        return std::string(data.begin(), data.end());
    }

private:
    std::string path; // 临时文件路径
};

/**
 * @brief 生成随机内容
 * 
 * @param size 内容大小（字节）
 * @return std::vector<uint8_t> 随机二进制数据
 */
inline std::vector<uint8_t> GenerateRandomData(size_t size) {
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dis(0, 255);
    
    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(dis(gen));
    }
    return data;
}

/**
 * @brief 生成随机字符串
 * 
 * @param length 字符串长度
 * @return std::string 随机字符串
 */
inline std::string GenerateRandomString(size_t length) {
    static const char charset[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "测试日本語한글☺✓";
    
    std::string result;
    result.reserve(length);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, sizeof(charset) - 2); // -2 避免终止符
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }
    
    return result;
}

/**
 * @brief 平台特定的测试标记
 * 
 * 在测试用例中用于标记平台特定的测试行为
 */
struct PlatformTestTag {
#if CP_PLATFORM_WINDOWS
    static constexpr const char* OS = "Windows";
    static constexpr bool HasRegistry = true;
    static constexpr bool CaseSensitiveFS = false;
#elif CP_PLATFORM_MACOS
    static constexpr const char* OS = "macOS";
    static constexpr bool HasRegistry = false;
    static constexpr bool CaseSensitiveFS = false; // HFS+ 不区分大小写，APFS 可配置
#elif CP_PLATFORM_LINUX
    static constexpr const char* OS = "Linux";
    static constexpr bool HasRegistry = false;
    static constexpr bool CaseSensitiveFS = true;
#else
    static constexpr const char* OS = "Unknown";
    static constexpr bool HasRegistry = false;
    static constexpr bool CaseSensitiveFS = true;
#endif
};

/**
 * @brief 比较两个文件是否相同
 * 
 * @param path1 第一个文件路径
 * @param path2 第二个文件路径
 * @return bool 文件内容是否相同
 */
inline bool CompareFiles(const std::string& path1, const std::string& path2) {
    auto content1 = CorePlatform::FileSystem::ReadFile(path1);
    auto content2 = CorePlatform::FileSystem::ReadFile(path2);
    
    if (content1.size() != content2.size()) {
        return false;
    }
    
    return std::equal(content1.begin(), content1.end(), content2.begin());
}

/**
 * @brief 日志捕获器 - 重定向std::cout输出到内存缓冲区
 * 
 * 用于测试控制台输出功能，构造时开始捕获，析构时恢复原状
 */
class LogCapture {
public:
    LogCapture() {
        original_buf = std::cout.rdbuf();
        std::cout.rdbuf(buffer.rdbuf());
    }
    
    ~LogCapture() {
        std::cout.rdbuf(original_buf);
    }
    
    std::string GetOutput() const {
        return buffer.str();
    }
    
    void Clear() {
        buffer.str("");
    }

private:
    std::streambuf* original_buf;
    std::stringstream buffer;
};

/**
 * @brief 日志内容验证器
 * 
 * @param logs 日志行集合
 * @param expectedLevel 期望的日志级别
 * @param messagePart 期望包含的消息片段
 * @param count 期望出现的次数（默认为1）
 */
inline void VerifyLogEntry(const std::vector<std::string>& logs, 
                         CorePlatform::LogLevel expectedLevel, 
                         const std::string& messagePart,
                         int count = 1) {
    std::string levelStr;
    switch (expectedLevel) {
        case CorePlatform::LogLevel::TRACE: levelStr = "TRACE"; break;
        case CorePlatform::LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case CorePlatform::LogLevel::INFO: levelStr = "INFO"; break;
        case CorePlatform::LogLevel::WARN: levelStr = "WARN"; break;
        case CorePlatform::LogLevel::ERR: levelStr = "ERROR"; break;
        case CorePlatform::LogLevel::FATAL: levelStr = "FATAL"; break;
    }
    
    int found = 0;
    for (const auto& log : logs) {
        if (log.find(levelStr) != std::string::npos && 
            log.find(messagePart) != std::string::npos) {
            found++;
        }
    }
    
    ASSERT_EQ(found, count) << "Expected " << count << " log entries with level [" 
                          << levelStr << "] containing [" << messagePart 
                          << "], found " << found;
}

/**
 * @brief 分割字符串为行
 * 
 * @param content 要分割的字符串
 * @return std::vector<std::string> 分割后的行向量
 */
inline std::vector<std::string> SplitIntoLines(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

} // namespace CorePlatformTest