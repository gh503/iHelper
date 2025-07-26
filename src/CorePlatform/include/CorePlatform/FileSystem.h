#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <system_error>
#include "CorePlatform/Export.h"

namespace CorePlatform {

class CORE_PLATFORM_API FileSystem {
public:
    // 文件打开模式
    enum class OpenMode {
        Read,
        Write,
        Append,
        ReadWrite
    };
    
    // 文件锁定模式
    enum class LockMode {
        None,
        Shared,     // 共享锁（读锁）
        Exclusive   // 独占锁（写锁）
    };
    
    // 文件类型
    enum class FileType {
        Regular,    // 普通文件
        Directory,  // 目录
        Symlink,    // 符号链接
        Other       // 其他类型
    };

    enum class Permissions {
        NONE = 0,           ///< 无权限
        
        // 所有者权限
        OWNER_READ = 0400,   ///< 所有者读权限
        OWNER_WRITE = 0200,  ///< 所有者写权限
        OWNER_EXEC = 0100,   ///< 所有者执行权限
        OWNER_ALL = 0700,    ///< 所有者所有权限 (读+写+执行)
        
        // 组权限
        GROUP_READ = 040,    ///< 组读权限
        GROUP_WRITE = 020,   ///< 组写权限
        GROUP_EXEC = 010,    ///< 组执行权限
        GROUP_ALL = 070,     ///< 组所有权限
        
        // 其他用户权限
        OTHERS_READ = 04,    ///< 其他用户读权限
        OTHERS_WRITE = 02,   ///< 其他用户写权限
        OTHERS_EXEC = 01,    ///< 其他用户执行权限
        OTHERS_ALL = 07,     ///< 其他用户所有权限
        
        // 特殊权限
        SET_UID = 04000,     ///< 设置用户ID
        SET_GID = 02000,     ///< 设置组ID
        STICKY_BIT = 01000,  ///< 粘滞位
        
        // 所有权限
        ALL = 07777,         ///< 所有权限 (所有者+组+其他+特殊)
        
        // 常用组合
        READ_ONLY = OWNER_READ | GROUP_READ | OTHERS_READ,
        READ_WRITE = READ_ONLY | OWNER_WRITE | GROUP_WRITE | OTHERS_WRITE,
        READ_EXEC = READ_ONLY | OWNER_EXEC | GROUP_EXEC | OTHERS_EXEC,
        READ_WRITE_EXEC = READ_WRITE | READ_EXEC
    };

    /**
     * @brief 设置文件或目录权限
     * 
     * @param path 文件或目录路径
     * @param permissions 权限位掩码 (使用Permissions枚举)
     * @return bool 是否成功设置权限
     * 
     * 示例:
     *   // 设置文件为所有者读写，其他用户只读
     *   FileSystem::SetPermissions("file.txt", 
     *       FileSystem::Permissions::OWNER_READ | 
     *       FileSystem::Permissions::OWNER_WRITE |
     *       FileSystem::Permissions::OTHERS_READ);
     */
    static bool SetPermissions(const std::string& path, int permissions);

    /**
     * @brief 获取文件或目录权限
     * 
     * @param path 文件或目录路径
     * @return int 当前权限位掩码，如果失败返回-1
     */
    static int GetPermissions(const std::string& path);

    /**
     * @brief 添加权限标志
     * 
     * @param path 文件或目录路径
     * @param permissions 要添加的权限
     * @return bool 是否成功
     */
    static bool AddPermissions(const std::string& path, int permissions);
    
    /**
     * @brief 移除权限标志
     * 
     * @param path 文件或目录路径
     * @param permissions 要移除的权限
     * @return bool 是否成功
     */
    static bool RemovePermissions(const std::string& path, int permissions);

    // ===== 文件操作 =====
    
    // 检查文件或目录是否存在
    static bool Exists(const std::string& path);
    
    // 获取文件类型
    static FileType GetFileType(const std::string& path);
    
    // 检查是否是普通文件
    static bool IsRegularFile(const std::string& path);
    
    // 检查是否是目录
    static bool IsDirectory(const std::string& path);
    
    // 检查是否是符号链接
    static bool IsSymlink(const std::string& path);
    
    // 获取文件大小（字节）
    static uint64_t GetFileSize(const std::string& path);
    
    // 获取文件创建时间
    static std::chrono::system_clock::time_point GetCreationTime(const std::string& path);
    
    // 获取文件修改时间
    static std::chrono::system_clock::time_point GetModificationTime(const std::string& path);
    
    // 设置文件修改时间
    static bool SetModificationTime(const std::string& path, 
                                   const std::chrono::system_clock::time_point& time);
    
    // 读取整个文件内容（二进制）
    static std::vector<uint8_t> ReadFile(const std::string& path);
    
    // 写入整个文件内容（二进制）
    static bool WriteFile(const std::string& path, const std::vector<uint8_t>& data);
    
    // 追加内容到文件（二进制）
    static bool AppendFile(const std::string& path, const std::vector<uint8_t>& data);
    
    // 读取整个文件内容（文本）
    static std::string ReadTextFile(const std::string& path);
    
    // 写入整个文件内容（文本）
    static bool WriteTextFile(const std::string& path, const std::string& content);
    
    // 删除文件
    static bool RemoveFile(const std::string& path);
    
    // 复制文件
    static bool DuplicateFile(const std::string& from, const std::string& to, bool overwrite = false);
    
    // 移动/重命名文件
    static bool RelocateFile(const std::string& from, const std::string& to);
    
    // ===== 目录操作 =====
    
    // 创建目录
    static bool NewDirectory(const std::string& path);
    
    // 创建目录（包括所有父目录）
    static bool NewDirectories(const std::string& path);
    
    // 删除目录（空目录）
    static bool DeleteDirectory(const std::string& path);
    
    // 删除目录及其所有内容
    static bool DeleteDirectoriesRecursive(const std::string& path);
    
    // 列出目录内容
    static bool ListDirectory(const std::string& path, std::vector<std::string>& entries);
    
    // 递归列出目录内容
    static bool ListDirectoryRecursive(const std::string& path, std::vector<std::string>& entries);
    
    // ===== 符号链接操作 =====
    
    // 创建符号链接
    static bool CreateSymlink(const std::string& target, const std::string& linkPath);
    
    // 读取符号链接目标
    static std::string ReadSymlink(const std::string& linkPath);
    
    // ===== 路径操作 =====
    
    // 获取当前工作目录
    static std::string GetCurDirectory();
    
    // 设置当前工作目录
    static bool SetCurDirectory(const std::string& path);
    
    // 获取临时目录
    static std::string GetTempDirectory();
    
    // 获取可执行文件路径
    static std::string GetExecutablePath();
    
    // 拼接路径
    static std::string PathJoin(const std::vector<std::string>& parts);
    
    // 获取文件名（含扩展名）
    static std::string GetFileName(const std::string& path);
    
    // 获取父目录
    static std::string GetParentDirectory(const std::string& path);
    
    // 获取文件扩展名
    static std::string GetFileExtension(const std::string& path);
    
    // 更改文件扩展名
    static std::string ChangeFileExtension(const std::string& path, const std::string& newExtension);
    
    // 获取绝对路径
    static std::string GetAbsolutePath(const std::string& path);
    
    // 规范化路径
    static std::string NormalizePath(const std::string& path);
    
    // ===== 文件锁定 =====
    
    // 文件锁作用域类
    class CORE_PLATFORM_API ScopedFileLock {
    public:
        ScopedFileLock(const std::string& path, LockMode mode);
        ~ScopedFileLock();
        
        bool IsLocked() const { return isLocked; }
        bool TryLock();
        void Unlock();
        
        // 禁用拷贝和移动
        ScopedFileLock(const ScopedFileLock&) = delete;
        ScopedFileLock& operator=(const ScopedFileLock&) = delete;
        ScopedFileLock(ScopedFileLock&&) = delete;
        ScopedFileLock& operator=(ScopedFileLock&&) = delete;
        
    private:
        std::string filePath;
        LockMode lockMode;
        bool isLocked;
        
        #if CP_PLATFORM_WINDOWS
            void* fileHandle = nullptr;
        #else
            int fileDescriptor = -1;
        #endif
    };
};

} // namespace CorePlatform