#include "CorePlatform/FileSystem.h"
#include "CorePlatform/Windows/WindowsUtils.h"
#include <Windows.h>
#include <winioctl.h>
#include <fileapi.h>
#include <shlwapi.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <memory>

#pragma comment(lib, "Shlwapi.lib")

namespace CorePlatform {

namespace {

// 转换时间结构
std::chrono::system_clock::time_point FileTimeToTimePoint(const FILETIME& ft) {
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    
    // 转换为UNIX时间（1601-01-01到1970-01-01的100纳秒间隔）
    constexpr uint64_t EPOCH_DIFFERENCE = 116444736000000000ULL;
    uint64_t totalNanoseconds = (ull.QuadPart - EPOCH_DIFFERENCE) * 100;
    
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::nanoseconds(totalNanoseconds)
        )
    );
}

FILETIME TimePointToFileTime(const std::chrono::system_clock::time_point& tp) {
    // 转换为1601-01-01以来的100纳秒间隔
    constexpr uint64_t EPOCH_DIFFERENCE = 116444736000000000ULL;
    auto duration = tp.time_since_epoch();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    uint64_t total100ns = EPOCH_DIFFERENCE + nanoseconds.count() / 100;
    
    FILETIME ft;
    ft.dwLowDateTime = static_cast<DWORD>(total100ns);
    ft.dwHighDateTime = static_cast<DWORD>(total100ns >> 32);
    return ft;
}

// 递归创建目录
bool CreateDirectoryRecursive(const std::wstring& wpath) {
    // 尝试直接创建目录
    if (CreateDirectoryW(wpath.c_str(), NULL)) {
        return true;
    }
    
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) {
        // 检查是否真的是目录
        DWORD attrib = GetFileAttributesW(wpath.c_str());
        return (attrib != INVALID_FILE_ATTRIBUTES) && 
               (attrib & FILE_ATTRIBUTE_DIRECTORY);
    }
    
    // 如果路径不存在，尝试创建父目录
    if (err == ERROR_PATH_NOT_FOUND) {
        size_t pos = wpath.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            std::wstring parent = wpath.substr(0, pos);
            if (!parent.empty() && CreateDirectoryRecursive(parent)) {
                return CreateDirectoryW(wpath.c_str(), NULL) != 0;
            }
        }
    }
    
    return false;
}

// 递归删除目录
bool DeleteDirectoryRecursive(const std::wstring& wpath) {
    // 首先删除目录内容
    WIN32_FIND_DATAW findData;
    std::wstring searchPath = wpath + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    bool success = true;
    do {
        // 跳过 "." 和 ".."
        if (wcscmp(findData.cFileName, L".") == 0 || 
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        std::wstring fullPath = wpath + L"\\" + findData.cFileName;
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // 递归删除子目录
            if (!DeleteDirectoryRecursive(fullPath)) {
                success = false;
            }
        } else {
            // 删除文件
            if (!DeleteFileW(fullPath.c_str())) {
                success = false;
            }
        }
    } while (FindNextFileW(hFind, &findData) != 0);
    
    FindClose(hFind);
    
    // 最后删除空目录
    if (success) {
        success = RemoveDirectoryW(wpath.c_str()) != 0;
    }
    
    return success;
}

} // 匿名命名空间

bool FileSystem::Exists(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    DWORD attrib = GetFileAttributesW(wpath.c_str());
    return attrib != INVALID_FILE_ATTRIBUTES;
}

FileSystem::FileType FileSystem::GetFileType(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    DWORD attrib = GetFileAttributesW(wpath.c_str());
    
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        return FileSystem::FileType::Other;
    }
    
    if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
        return FileSystem::FileType::Directory;
    }
    
    if (attrib & FILE_ATTRIBUTE_REPARSE_POINT) {
        // 检查是否是符号链接
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(wpath.c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            FindClose(hFind);
            if (findData.dwReserved0 == IO_REPARSE_TAG_SYMLINK) {
                return FileSystem::FileType::Symlink;
            }
        }
        return FileSystem::FileType::Other;
    }
    
    return FileSystem::FileType::Regular;
}

bool FileSystem::IsRegularFile(const std::string& path) {
    return GetFileType(path) == FileSystem::FileType::Regular;
}

bool FileSystem::IsDirectory(const std::string& path) {
    return GetFileType(path) == FileSystem::FileType::Directory;
}

bool FileSystem::IsSymlink(const std::string& path) {
    return GetFileType(path) == FileSystem::FileType::Symlink;
}

uint64_t FileSystem::GetFileSize(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    WIN32_FILE_ATTRIBUTE_DATA fileAttr;
    
    if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &fileAttr)) {
        return 0;
    }
    
    ULARGE_INTEGER size;
    size.LowPart = fileAttr.nFileSizeLow;
    size.HighPart = fileAttr.nFileSizeHigh;
    return size.QuadPart;
}

std::chrono::system_clock::time_point FileSystem::GetCreationTime(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    WIN32_FILE_ATTRIBUTE_DATA fileAttr;
    
    if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &fileAttr)) {
        return std::chrono::system_clock::time_point();
    }
    
    return FileTimeToTimePoint(fileAttr.ftCreationTime);
}

std::chrono::system_clock::time_point FileSystem::GetModificationTime(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    WIN32_FILE_ATTRIBUTE_DATA fileAttr;
    
    if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &fileAttr)) {
        return std::chrono::system_clock::time_point();
    }
    
    return FileTimeToTimePoint(fileAttr.ftLastWriteTime);
}

bool FileSystem::SetModificationTime(const std::string& path, 
                                      const std::chrono::system_clock::time_point& time) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    HANDLE hFile = CreateFileW(wpath.c_str(), FILE_WRITE_ATTRIBUTES, 
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    FILETIME ft = TimePointToFileTime(time);
    bool success = SetFileTime(hFile, NULL, NULL, &ft) != 0;
    CloseHandle(hFile);
    return success;
}

std::vector<uint8_t> FileSystem::ReadFile(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return {};
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return {};
    }
    
    if (fileSize.QuadPart > static_cast<LONGLONG>(std::numeric_limits<DWORD>::max())) {
        CloseHandle(hFile);
        return {};
    }
    
    std::vector<uint8_t> buffer(fileSize.QuadPart);
    DWORD bytesRead = 0;
    
    if (!::ReadFile(hFile, buffer.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, NULL)) {
        CloseHandle(hFile);
        return {};
    }
    
    CloseHandle(hFile);
    return buffer;
}

bool FileSystem::WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0, 
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD bytesWritten = 0;
    bool success = ::WriteFile(hFile, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, NULL) != 0;
    CloseHandle(hFile);
    
    return success && (bytesWritten == data.size());
}

bool FileSystem::AppendFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    HANDLE hFile = CreateFileW(wpath.c_str(), FILE_APPEND_DATA, 
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD bytesWritten = 0;
    bool success = ::WriteFile(hFile, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, NULL) != 0;
    CloseHandle(hFile);
    
    return success && (bytesWritten == data.size());
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
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    return DeleteFileW(wpath.c_str()) != 0;
}

bool FileSystem::DuplicateFile(const std::string& from, const std::string& to, bool overwrite) {
    std::wstring wfrom = WindowsUtils::UTF8ToWide(from);
    std::wstring wto = WindowsUtils::UTF8ToWide(to);
    return CopyFileW(wfrom.c_str(), wto.c_str(), overwrite ? FALSE : TRUE) != 0;
}

bool FileSystem::RelocateFile(const std::string& from, const std::string& to) {
    std::wstring wfrom = WindowsUtils::UTF8ToWide(from);
    std::wstring wto = WindowsUtils::UTF8ToWide(to);
    return MoveFileExW(wfrom.c_str(), wto.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

// ===== 目录操作 =====

bool FileSystem::NewDirectory(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    return CreateDirectoryW(wpath.c_str(), NULL) != 0;
}

bool FileSystem::NewDirectories(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    return CreateDirectoryRecursive(wpath);
}

bool FileSystem::DeleteDirectory(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    return RemoveDirectoryW(wpath.c_str()) != 0;
}

bool FileSystem::DeleteDirectoriesRecursive(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    return DeleteDirectoryRecursive(wpath);
}

bool FileSystem::ListDirectory(const std::string& path, std::vector<std::string>& entries) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    std::wstring searchPath = wpath + L"\\*";
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    entries.clear();
    do {
        // 跳过 "." 和 ".."
        if (wcscmp(findData.cFileName, L".") == 0 || 
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        entries.push_back(WindowsUtils::WideToUTF8(findData.cFileName));
    } while (FindNextFileW(hFind, &findData) != 0);
    
    FindClose(hFind);
    return true;
}

bool FileSystem::ListDirectoryRecursive(const std::string& path, std::vector<std::string>& entries) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    std::wstring searchPath = wpath + L"\\*";
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    do {
        // 跳过 "." 和 ".."
        if (wcscmp(findData.cFileName, L".") == 0 || 
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        std::wstring fullPath = wpath + L"\\" + findData.cFileName;
        std::string utf8Path = WindowsUtils::WideToUTF8(fullPath);
        entries.push_back(utf8Path);
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // 递归处理子目录
            std::vector<std::string> subEntries;
            if (ListDirectoryRecursive(utf8Path, subEntries)) {
                entries.insert(entries.end(), subEntries.begin(), subEntries.end());
            }
        }
    } while (FindNextFileW(hFind, &findData) != 0);
    
    FindClose(hFind);
    return true;
}

// ===== 符号链接操作 =====

bool FileSystem::CreateSymlink(const std::string& target, const std::string& linkPath) {
    std::wstring wtarget = WindowsUtils::UTF8ToWide(target);
    std::wstring wlinkPath = WindowsUtils::UTF8ToWide(linkPath);
    
    // 确定目标类型（文件或目录）
    DWORD flags = 0;
    DWORD attrib = GetFileAttributesW(wtarget.c_str());
    
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    
    if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
        flags = SYMBOLIC_LINK_FLAG_DIRECTORY;
    }
    
    // 需要管理员权限或开发者模式
    return CreateSymbolicLinkW(wlinkPath.c_str(), wtarget.c_str(), flags) != 0;
}

std::string FileSystem::ReadSymlink(const std::string& linkPath) {
    std::wstring wlinkPath = WindowsUtils::UTF8ToWide(linkPath);
    
    // 使用 Windows API 提供的符号链接读取函数
    wchar_t target[MAX_PATH];
    if (GetFinalPathNameByHandleW(
        CreateFileW(wlinkPath.c_str(), 0, 0, NULL, OPEN_EXISTING, 
                   FILE_FLAG_BACKUP_SEMANTICS, NULL),
        target, MAX_PATH, VOLUME_NAME_NT) == 0) 
    {
        return "";
    }
    
    std::wstring wtarget(target);
    // 移除可能的 "\\?\" 前缀
    if (wtarget.find(L"\\\\?\\") == 0) {
        wtarget = wtarget.substr(4);
    }
    
    return WindowsUtils::WideToUTF8(wtarget);
}

// ===== 路径操作 =====

std::string FileSystem::GetCurDirectory() {
    DWORD size = GetCurrentDirectoryW(0, NULL);
    if (size == 0) return "";
    
    std::vector<wchar_t> buffer(size);
    GetCurrentDirectoryW(size, buffer.data());
    
    return WindowsUtils::WideToUTF8(buffer.data());
}

bool FileSystem::SetCurDirectory(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    return SetCurrentDirectoryW(wpath.c_str()) != 0;
}

std::string FileSystem::GetTempDirectory() {
    wchar_t buffer[MAX_PATH];
    DWORD result = GetTempPathW(MAX_PATH, buffer);
    if (result == 0 || result > MAX_PATH) {
        return "";
    }
    return WindowsUtils::WideToUTF8(buffer);
}

std::string FileSystem::GetExecutablePath() {
    wchar_t buffer[MAX_PATH];
    DWORD result = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (result == 0 || result == MAX_PATH) {
        return "";
    }
    return WindowsUtils::WideToUTF8(buffer);
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
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    wchar_t fname[_MAX_FNAME];
    wchar_t ext[_MAX_EXT];
    
    _wsplitpath_s(wpath.c_str(), NULL, 0, NULL, 0, fname, _MAX_FNAME, ext, _MAX_EXT);
    
    std::wstring result = fname;
    result += ext;
    return WindowsUtils::WideToUTF8(result);
}

std::string FileSystem::GetParentDirectory(const std::string& path) {
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    wchar_t drive[_MAX_DRIVE];
    wchar_t dir[_MAX_DIR];
    
    _wsplitpath_s(wpath.c_str(), drive, _MAX_DRIVE, dir, _MAX_DIR, NULL, 0, NULL, 0);
    
    std::wstring result = drive;
    result += dir;
    
    // 移除尾部反斜杠
    if (!result.empty() && (result.back() == L'\\' || result.back() == L'/')) {
        result.pop_back();
    }
    
    return WindowsUtils::WideToUTF8(result);
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
    std::wstring wpath = WindowsUtils::UTF8ToWide(path);
    wchar_t fullPath[MAX_PATH];
    
    DWORD result = GetFullPathNameW(wpath.c_str(), MAX_PATH, fullPath, NULL);
    if (result == 0 || result > MAX_PATH) {
        return path; // 返回原路径作为后备
    }
    
    return WindowsUtils::WideToUTF8(fullPath);
}

std::string FileSystem::NormalizePath(const std::string& path) {
    std::string absPath = GetAbsolutePath(path);
    std::replace(absPath.begin(), absPath.end(), '/', '\\');
    
    // 转换为小写（Windows路径不区分大小写）
    std::transform(absPath.begin(), absPath.end(), absPath.begin(), 
                  [](unsigned char c) { return std::tolower(c); });
    
    return absPath;
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

    std::wstring wpath = WindowsUtils::UTF8ToWide(filePath);
    DWORD access = (lockMode == FileSystem::LockMode::Exclusive) ? GENERIC_WRITE : GENERIC_READ;
    DWORD shareMode = (lockMode == FileSystem::LockMode::Exclusive) ? 0 : FILE_SHARE_READ;
    
    HANDLE hFile = CreateFileW(wpath.c_str(), access, shareMode, 
                              NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));
    BOOL result;
    
    if (lockMode == FileSystem::LockMode::Exclusive) {
        result = LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &overlapped);
    } else {
        result = LockFileEx(hFile, 0, 0, MAXDWORD, MAXDWORD, &overlapped);
    }
    
    if (!result) {
        CloseHandle(hFile);
        return false;
    }
    
    fileHandle = hFile;
    isLocked = true;
    return true;
}

void FileSystem::ScopedFileLock::Unlock() {
    if (!fileHandle) return;
    if (!isLocked) return;
    
    HANDLE hFile = static_cast<HANDLE>(fileHandle);
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));
    UnlockFileEx(hFile, 0, MAXDWORD, MAXDWORD, &overlapped);
    CloseHandle(hFile);

    isLocked = false;
}

} // namespace CorePlatform