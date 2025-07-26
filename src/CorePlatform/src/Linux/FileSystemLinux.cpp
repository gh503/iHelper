#include "CorePlatform/FileSystem.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cerrno>
#include <utime.h>
#include <sys/time.h>
#include <pwd.h>
#include <stdexcept>

namespace CorePlatform {

namespace {

// 转换时间结构
std::chrono::system_clock::time_point TimeSpecToTimePoint(const struct timespec& ts) {
    return std::chrono::system_clock::time_point(
        std::chrono::seconds(ts.tv_sec) +
        std::chrono::nanoseconds(ts.tv_nsec)
    );
}

struct timespec TimePointToTimeSpec(const std::chrono::system_clock::time_point& tp) {
    auto duration = tp.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - secs);
    
    struct timespec ts;
    ts.tv_sec = secs.count();
    ts.tv_nsec = ns.count();
    return ts;
}

// 递归创建目录
bool CreateDirectoryRecursive(const std::string& path, mode_t mode = 0755) {
    if (path.empty()) return false;
    
    // 检查是否已存在
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    
    // 递归创建父目录
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos) {
        std::string parent = path.substr(0, pos);
        if (!parent.empty() && !CreateDirectoryRecursive(parent, mode)) {
            return false;
        }
    }
    
    // 创建当前目录
    return mkdir(path.c_str(), mode) == 0;
}

// 递归删除目录
bool DeleteDirectoryRecursive(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) return false;
    
    bool success = true;
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string fullPath = path + "/" + entry->d_name;
        struct stat statbuf;
        
        if (lstat(fullPath.c_str(), &statbuf) != 0) {
            success = false;
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            // 递归删除子目录
            if (!DeleteDirectoryRecursive(fullPath)) {
                success = false;
            }
        } else {
            // 删除文件
            if (unlink(fullPath.c_str()) != 0) {
                success = false;
            }
        }
    }
    
    closedir(dir);
    
    // 删除空目录
    if (success) {
        success = rmdir(path.c_str()) == 0;
    }
    
    return success;
}

} // 匿名命名空间

bool FileSystem::Exists(const std::string& path) {
    struct stat st;
    return lstat(path.c_str(), &st) == 0;
}

FileSystem::FileType FileSystem::GetFileType(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        return FileSystem::FileType::Other;
    }
    
    if (S_ISREG(st.st_mode)) {
        return FileSystem::FileType::Regular;
    }
    
    if (S_ISDIR(st.st_mode)) {
        return FileSystem::FileType::Directory;
    }
    
    if (S_ISLNK(st.st_mode)) {
        return FileSystem::FileType::Symlink;
    }
    
    return FileSystem::FileType::Other;
}

bool FileSystem::IsRegularFile(const std::string& path) {
    struct stat st;
    return lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool FileSystem::IsDirectory(const std::string& path) {
    struct stat st;
    return lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool FileSystem::IsSymlink(const std::string& path) {
    struct stat st;
    return lstat(path.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
}

uint64_t FileSystem::GetFileSize(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        return 0;
    }
    return static_cast<uint64_t>(st.st_size);
}

std::chrono::system_clock::time_point FileSystem::GetCreationTime(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        return std::chrono::system_clock::time_point();
    }
    
    // Linux 没有创建时间，使用修改时间代替
    return TimeSpecToTimePoint(st.st_mtim);
}

std::chrono::system_clock::time_point FileSystem::GetModificationTime(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        return std::chrono::system_clock::time_point();
    }
    return TimeSpecToTimePoint(st.st_mtim);
}

bool FileSystem::SetModificationTime(const std::string& path, 
                                        const std::chrono::system_clock::time_point& time) {
    struct timespec times[2] = {
        {0, UTIME_OMIT},  // 不修改访问时间
        TimePointToTimeSpec(time)
    };
    
    return utimensat(AT_FDCWD, path.c_str(), times, 0) == 0;
}

std::vector<uint8_t> FileSystem::ReadFile(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        return {};
    }
    
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return {};
    }
    
    std::vector<uint8_t> buffer(st.st_size);
    ssize_t bytesRead = read(fd, buffer.data(), st.st_size);
    close(fd);
    
    if (bytesRead != st.st_size) {
        return {};
    }
    
    return buffer;
}

bool FileSystem::WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        return false;
    }
    
    ssize_t bytesWritten = write(fd, data.data(), data.size());
    close(fd);
    
    return bytesWritten == static_cast<ssize_t>(data.size());
}

bool FileSystem::AppendFile(const std::string& path, const std::vector<uint8_t>& data) {
    int fd = open(path.c_str(), O_WRONLY | O_APPEND);
    if (fd == -1) {
        return false;
    }
    
    ssize_t bytesWritten = write(fd, data.data(), data.size());
    close(fd);
    
    return bytesWritten == static_cast<ssize_t>(data.size());
}

std::string FileSystem::ReadTextFile(const std::string& path) {
    auto data = ReadFile(path);
    return std::string(data.begin(), data.end());
}

bool FileSystem::WriteTextFile(const std::string& path, const std::string& content) {
    std::vector<uint8_t> data(content.begin(), content.end());
    return WriteFile(path, data);
}

bool FileSystem::RemoveFile(const std::string& path) {
    return unlink(path.c_str()) == 0;
}

bool FileSystem::DuplicateFile(const std::string& from, const std::string& to, bool overwrite) {
    if (!overwrite && access(to.c_str(), F_OK) == 0) {
        errno = EEXIST;
        return false;
    }
    
    // 使用 Linux 特有的 sendfile 进行高效复制
    int source_fd = open(from.c_str(), O_RDONLY);
    if (source_fd == -1) {
        return false;
    }
    
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    if (!overwrite) {
        flags |= O_EXCL;
    }
    
    int dest_fd = open(to.c_str(), flags, 0644);
    if (dest_fd == -1) {
        close(source_fd);
        return false;
    }
    
    struct stat stat_source;
    if (fstat(source_fd, &stat_source) == -1) {
        close(source_fd);
        close(dest_fd);
        return false;
    }
    
    // 使用 sendfile 进行高效复制
    ssize_t result = sendfile(dest_fd, source_fd, nullptr, stat_source.st_size);
    
    close(source_fd);
    close(dest_fd);
    
    return result == stat_source.st_size;
}

bool FileSystem::RelocateFile(const std::string& from, const std::string& to) {
    return rename(from.c_str(), to.c_str()) == 0;
}

// ===== 目录操作 =====

bool FileSystem::NewDirectory(const std::string& path) {
    return mkdir(path.c_str(), 0755) == 0;
}

bool FileSystem::NewDirectories(const std::string& path) {
    return CreateDirectoryRecursive(path);
}

bool FileSystem::DeleteDirectory(const std::string& path) {
    return rmdir(path.c_str()) == 0;
}

bool FileSystem::DeleteDirectoriesRecursive(const std::string& path) {
    return DeleteDirectoryRecursive(path);
}

bool FileSystem::ListDirectory(const std::string& path, std::vector<std::string>& entries) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return false;
    }
    
    entries.clear();
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        entries.push_back(entry->d_name);
    }
    
    closedir(dir);
    return true;
}

bool FileSystem::ListDirectoryRecursive(const std::string& path, std::vector<std::string>& entries) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return false;
    }
    
    struct dirent* entry;
    
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string fullPath = path + "/" + entry->d_name;
        entries.push_back(fullPath);
        
        struct stat st;
        if (lstat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            // 递归处理子目录
            std::vector<std::string> subEntries;
            if (ListDirectoryRecursive(fullPath, subEntries)) {
                entries.insert(entries.end(), subEntries.begin(), subEntries.end());
            }
        }
    }
    
    closedir(dir);
    return true;
}

// ===== 符号链接操作 =====

bool FileSystem::CreateSymlink(const std::string& target, const std::string& linkPath) {
    return symlink(target.c_str(), linkPath.c_str()) == 0;
}

std::string FileSystem::ReadSymlink(const std::string& linkPath) {
    char buffer[PATH_MAX];
    ssize_t len = readlink(linkPath.c_str(), buffer, sizeof(buffer) - 1);
    if (len == -1) {
        return "";
    }
    buffer[len] = '\0';
    return buffer;
}

// ===== 路径操作 =====

std::string FileSystem::GetCurDirectory() {
    char buffer[PATH_MAX];
    if (getcwd(buffer, sizeof(buffer)) == nullptr) {
        return "";
    }
    return buffer;
}

bool FileSystem::SetCurDirectory(const std::string& path) {
    return chdir(path.c_str()) == 0;
}

std::string FileSystem::GetTempDirectory() {
    const char* temp = getenv("TMPDIR");
    if (temp) return temp;
    
    temp = getenv("TMP");
    if (temp) return temp;
    
    temp = getenv("TEMP");
    if (temp) return temp;
    
    return "/tmp"; // Linux 默认临时目录
}

std::string FileSystem::GetExecutablePath() {
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1) {
        return "";
    }
    path[len] = '\0';
    return path;
}

std::string FileSystem::PathJoin(const std::vector<std::string>& parts) {
    if (parts.empty()) return "";
    
    #if CP_PLATFORM_WINDOWS
        char separator = '\\';
    #else
        char separator = '/';
    #endif
    
    std::string result = parts[0];
    
    for (size_t i = 1; i < parts.size(); ++i) {
        if (result.back() != separator && !parts[i].empty() && parts[i][0] != separator) {
            result += separator;
        } else if (!result.empty() && result.back() == separator && 
                   !parts[i].empty() && parts[i][0] == separator) {
            // 避免双分隔符
            result.pop_back();
        }
        result += parts[i];
    }
    
    return result;
}

std::string FileSystem::GetFileName(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string FileSystem::GetParentDirectory(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos);
}

std::string FileSystem::GetFileExtension(const std::string& path) {
    size_t pos = path.find_last_of(".");
    if (pos == std::string::npos || pos == path.size() - 1) {
        return "";
    }
    
    size_t slashPos = path.find_last_of("/\\");
    if (slashPos != std::string::npos && slashPos > pos) {
        return "";
    }
    
    return path.substr(pos);
}

std::string FileSystem::ChangeFileExtension(const std::string& path, const std::string& newExtension) {
    std::string currentExt = GetFileExtension(path);
    if (currentExt.empty()) {
        return path + (newExtension.empty() ? "" : 
                      (newExtension[0] == '.' ? newExtension : "." + newExtension));
    }
    
    return path.substr(0, path.size() - currentExt.size()) + 
           (newExtension.empty() ? "" : 
           (newExtension[0] == '.' ? newExtension : "." + newExtension));
}

std::string FileSystem::GetAbsolutePath(const std::string& path) {
    if (path.empty()) {
        return "";
    }
    
    char absPath[PATH_MAX];
    if (realpath(path.c_str(), absPath) == nullptr) {
        return path; // 返回原路径作为后备
    }
    return absPath;
}

std::string FileSystem::NormalizePath(const std::string& path) {
    return GetAbsolutePath(path);
}

// ===== 文件锁定 =====

FileSystem::ScopedFileLock::ScopedFileLock(const std::string& path, LockMode mode) 
    : filePath(path), lockMode(mode), isLocked(false) {
    TryLock();
}

FileSystem::ScopedFileLock::~ScopedFileLock() {
    Unlock();
}

bool FileSystem::ScopedFileLock::TryLock() {
    if (isLocked) return true;

    int flags = O_RDWR | O_CREAT;
    if (lockMode == FileSystem::LockMode::Exclusive) {
        flags |= O_EXCL;
    }
    
    int fd = open(path.c_str(), flags, 0644);
    if (fd == -1) {
        return false;
    }
    
    struct flock fl;
    fl.l_type = (lockMode == FileSystem::LockMode::Exclusive) ? F_WRLCK : F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0; // 锁定整个文件
    
    if (fcntl(fd, F_SETLK, &fl) == -1) {
        close(fd);
        return false;
    }
    
    fileDescriptor = fd;
    isLocked = true;
    return true;
}

bool FileSystem::ScopedFileLock::Unlock() {
    if (fileDescriptor == -1) return;
    if (!isLocked) return;
    
    struct flock fl;
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    
    fcntl(fileDescriptor, F_SETLK, &fl);
    close(fileDescriptor);

    isLocked = false;
}

bool FileSystem::SetPermissions(const std::string& path, int permissions) {
    mode_t mode = 0;
    
    if (permissions & Permissions::OWNER_READ) mode |= S_IRUSR;
    if (permissions & Permissions::OWNER_WRITE) mode |= S_IWUSR;
    if (permissions & Permissions::OWNER_EXEC) mode |= S_IXUSR;
    
    if (permissions & Permissions::GROUP_READ) mode |= S_IRGRP;
    if (permissions & Permissions::GROUP_WRITE) mode |= S_IWGRP;
    if (permissions & Permissions::GROUP_EXEC) mode |= S_IXGRP;
    
    if (permissions & Permissions::OTHERS_READ) mode |= S_IROTH;
    if (permissions & Permissions::OTHERS_WRITE) mode |= S_IWOTH;
    if (permissions & Permissions::OTHERS_EXEC) mode |= S_IXOTH;
    
    if (permissions & Permissions::SET_UID) mode |= S_ISUID;
    if (permissions & Permissions::SET_GID) mode |= S_ISGID;
    if (permissions & Permissions::STICKY_BIT) mode |= S_ISVTX;
    
    return chmod(path.c_str(), mode) == 0;
}

int FileSystem::GetPermissions(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return -1;
    }
    
    int perms = 0;
    
    if (st.st_mode & S_IRUSR) perms |= Permissions::OWNER_READ;
    if (st.st_mode & S_IWUSR) perms |= Permissions::OWNER_WRITE;
    if (st.st_mode & S_IXUSR) perms |= Permissions::OWNER_EXEC;
    
    if (st.st_mode & S_IRGRP) perms |= Permissions::GROUP_READ;
    if (st.st_mode & S_IWGRP) perms |= Permissions::GROUP_WRITE;
    if (st.st_mode & S_IXGRP) perms |= Permissions::GROUP_EXEC;
    
    if (st.st_mode & S_IROTH) perms |= Permissions::OTHERS_READ;
    if (st.st_mode & S_IWOTH) perms |= Permissions::OTHERS_WRITE;
    if (st.st_mode & S_IXOTH) perms |= Permissions::OTHERS_EXEC;
    
    if (st.st_mode & S_ISUID) perms |= Permissions::SET_UID;
    if (st.st_mode & S_ISGID) perms |= Permissions::SET_GID;
    if (st.st_mode & S_ISVTX) perms |= Permissions::STICKY_BIT;
    
    return perms;
}

bool FileSystem::AddPermissions(const std::string& path, int permissions) {
    int current = GetPermissions(path);
    if (current == -1) return false;
    return SetPermissions(path, current | permissions);
}

bool FileSystem::RemovePermissions(const std::string& path, int permissions) {
    int current = GetPermissions(path);
    if (current == -1) return false;
    return SetPermissions(path, current & ~permissions);
}

} // namespace CorePlatform