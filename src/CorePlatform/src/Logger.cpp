#include "CorePlatform/Logger.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <cctype>

// 平台相关头文件
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#endif

namespace CorePlatform {

Logger::Logger() : 
    currentLevel_(LogLevel::INFO), 
    consoleOutput_(true),
    logFilePath_("") 
{
    // 确保日志文件目录存在
    if (!logFilePath_.empty()) {
        std::filesystem::path dir = std::filesystem::path(logFilePath_).parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
    }
}

Logger::~Logger() {
    if (fileStream_ && fileStream_->is_open()) {
        fileStream_->close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLevel(LogLevel level) {
    currentLevel_ = level;
}

void Logger::setConsoleOutput(bool enable) {
    consoleOutput_ = enable;
}

bool Logger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (fileStream_ && fileStream_->is_open()) {
        fileStream_->close();
    }
    
    // 确保目录存在
    std::filesystem::path dir = std::filesystem::path(path).parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }
    logFilePath_ = path;

#ifdef _WIN32
    // Windows 专用: 使用共享模式打开
    std::wstring widePath(path.begin(), path.end());
    int fd = _wsopen(
        widePath.c_str(),
        _O_WRONLY | _O_CREAT | _O_APPEND,
        _SH_DENYNO,  // 允许其他进程读取
        _S_IREAD | _S_IWRITE
    );
    
    if (fd == -1) {
        // 尝试创建目录
        std::filesystem::path pathObj(path);
        std::filesystem::create_directories(pathObj.parent_path());
        
        // 再次尝试打开
        fd = _wsopen(
            widePath.c_str(),
            _O_WRONLY | _O_CREAT | _O_APPEND,
            _SH_DENYNO,
            _S_IREAD | _S_IWRITE
        );
        
        if (fd == -1) {
            std::cerr << "Failed to open log file: " << path << std::endl;
            return false;
        }
    }
    
    // 使用文件描述符创建 FILE*
    FILE* file = _fdopen(fd, "a");
    if (!file) {
        _close(fd);
        return false;
    }
    
    // 创建新的 ofstream 并管理在 unique_ptr 中
    fileStream_ = std::make_unique<std::ofstream>(file);
#else
    // 非 Windows 系统使用标准方式
    fileStream_ = std::make_unique<std::ofstream>(path, std::ios::out | std::ios::app);
#endif
    
    if (!fileStream_->is_open()) {
        fileStream_.reset();
        return false;
    }
    return true;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel_) return;
    logInternal(level, message);
}

void Logger::trace(const std::string& message) { log(LogLevel::TRACE, message); }
void Logger::debug(const std::string& message) { log(LogLevel::DEBUG, message); }
void Logger::info(const std::string& message)  { log(LogLevel::INFO, message);  }
void Logger::warn(const std::string& message)  { log(LogLevel::WARN, message);  }
void Logger::error(const std::string& message) { log(LogLevel::ERR, message); }
void Logger::fatal(const std::string& message) { log(LogLevel::FATAL, message); }

std::string Logger::getCurrentTime() const {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::getLevelString(LogLevel level) const {
    switch(level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

void Logger::setConsoleColor(LogLevel level) const {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD color = 0;
    
    switch(level) {
        case LogLevel::FATAL: color = BACKGROUND_RED | FOREGROUND_INTENSITY; break;
        case LogLevel::ERR: color = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
        case LogLevel::WARN:  color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case LogLevel::INFO:  color = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case LogLevel::DEBUG: color = FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        case LogLevel::TRACE: color = FOREGROUND_BLUE | FOREGROUND_GREEN; break;
        default: color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }
    
    SetConsoleTextAttribute(hConsole, color);
#else
    const char* colorCode = "";
    
    switch(level) {
        case LogLevel::FATAL: colorCode = "\033[41;37m"; break; // 红底白字
        case LogLevel::ERR: colorCode = "\033[31;1m";  break; // 红色加粗
        case LogLevel::WARN:  colorCode = "\033[33;1m";  break; // 黄色加粗
        case LogLevel::INFO:  colorCode = "\033[32;1m";  break; // 绿色加粗
        case LogLevel::DEBUG: colorCode = "\033[34;1m";  break; // 蓝色加粗
        case LogLevel::TRACE: colorCode = "\033[36m";    break; // 青色
        default: colorCode = "\033[0m";
    }
    
    std::cout << colorCode;
#endif
}

void Logger::resetConsoleColor() const {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    std::cout << "\033[0m";
#endif
}

void Logger::logInternal(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    std::string timeStr = getCurrentTime();
    std::string levelStr = getLevelString(level);
    std::string logEntry = "[" + timeStr + "] [" + levelStr + "] " + message;
    
    // 控制台输出
    if (consoleOutput_) {
        setConsoleColor(level);
        std::cout << logEntry << std::endl;
        resetConsoleColor();
    }
    
    // 文件输出
    if (fileStream_ && fileStream_->is_open()) {
        *fileStream_ << logEntry << std::endl;
        fileStream_->flush();
    }
}

std::vector<std::string> Logger::readLogFileUnlocked() const {
    std::vector<std::string> lines;
    
    if (logFilePath_.empty()) {
        return lines;
    }
    
    std::ifstream file(logFilePath_);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open log file: " + logFilePath_);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    
    return lines;
}

std::vector<std::string> Logger::readAllLogs() const {
    std::lock_guard<std::mutex> lock(logMutex_);
    return readLogFileUnlocked();
}

std::vector<std::string> Logger::readLogsFromBeginning(int maxLines) const {
    std::lock_guard<std::mutex> lock(logMutex_);
    auto lines = readLogFileUnlocked();
    
    if (maxLines > 0 && static_cast<int>(lines.size()) > maxLines) {
        lines.resize(maxLines);
    }
    
    return lines;
}

std::vector<std::string> Logger::readLogsFromEnd(int maxLines) const {
    std::lock_guard<std::mutex> lock(logMutex_);
    auto lines = readLogFileUnlocked();
    
    if (maxLines > 0 && static_cast<int>(lines.size()) > maxLines) {
        lines.erase(lines.begin(), lines.end() - maxLines);
    }
    
    return lines;
}

std::vector<std::string> Logger::readLogsInRange(int startLine, int endLine) const {
    std::lock_guard<std::mutex> lock(logMutex_);
    auto lines = readLogFileUnlocked();
    
    if (lines.empty()) return {};
    
    const int totalLines = static_cast<int>(lines.size());
    
    // 处理非法值0：视为第1行
    if (startLine == 0) startLine = 1;
    if (endLine == 0) endLine = 1;
    
    // 转换1-based为0-based索引（正数-1，负数直接加总行数）
    auto toZeroBased = [totalLines](int line) {
        return line > 0 ? line - 1 : totalLines + line;
    };
    
    int startIndex = toZeroBased(startLine);
    int endIndex = toZeroBased(endLine);
    
    // 边界检查
    startIndex = std::max(0, startIndex);
    endIndex = std::min(totalLines - 1, endIndex);
    
    if (startIndex > endIndex) return {};
    
    std::vector<std::string> result;
    result.reserve(endIndex - startIndex + 1); // 预分配内存
    for (int i = startIndex; i <= endIndex; ++i) {
        result.push_back(lines[i]);
    }
    
    return result;
}

bool Logger::containsInLogs(const std::string& searchText, 
                          int startLine, 
                          int endLine) const {
    std::lock_guard<std::mutex> lock(logMutex_);
    auto lines = readLogFileUnlocked();
    
    if (lines.empty()) return false;
    
    const int totalLines = static_cast<int>(lines.size());
    
    // 处理非法值0：视为第1行
    if (startLine == 0) startLine = 1;
    if (endLine == 0) endLine = 1;
    
    // 转换1-based为0-based索引
    auto toZeroBased = [totalLines](int line) {
        return line > 0 ? line - 1 : totalLines + line;
    };
    
    int startIndex = toZeroBased(startLine);
    int endIndex = toZeroBased(endLine);
    
    // 边界检查
    startIndex = std::max(0, startIndex);
    endIndex = std::min(totalLines - 1, endIndex);
    
    if (startIndex > endIndex) return false;
    
    // 大小写不敏感搜索（优化：避免重复转换搜索文本）
    std::string lowerSearch;
    lowerSearch.reserve(searchText.size());
    std::transform(searchText.begin(), searchText.end(), 
                  std::back_inserter(lowerSearch),
                  [](unsigned char c){ return std::tolower(c); });
    
    for (int i = startIndex; i <= endIndex; ++i) {
        // 避免修改原字符串
        std::string lowerLine;
        lowerLine.reserve(lines[i].size());
        std::transform(lines[i].begin(), lines[i].end(),
                      std::back_inserter(lowerLine),
                      [](unsigned char c){ return std::tolower(c); });
        
        if (lowerLine.find(lowerSearch) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

// 刷新缓冲区
void Logger::flush() {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (fileStream_ && fileStream_->is_open()) {
        fileStream_->flush();
    }
}

// 获取当前日志路径
std::string Logger::getCurrentLogPath() const {
    std::lock_guard<std::mutex> lock(logMutex_);
    return logFilePath_;
} 

}