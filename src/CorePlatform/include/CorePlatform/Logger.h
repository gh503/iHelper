#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include "CorePlatform/Export.h"

namespace CorePlatform {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERR,
    FATAL
};

class CORE_PLATFORM_API Logger {
public:
    // 获取单例实例
    static Logger& getInstance();
    
    // 设置日志级别
    void setLevel(LogLevel level);
    
    // 启用/禁用控制台输出
    void setConsoleOutput(bool enable);
    
    // 设置日志文件路径
    bool setLogFile(const std::string& path);
    
    // 日志记录接口
    void log(LogLevel level, const std::string& message);
    
    // 便捷方法
    void trace(const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
    
    // 日志读取功能增强
    std::vector<std::string> readAllLogs() const;
    std::vector<std::string> readLogsFromBeginning(int maxLines) const;
    std::vector<std::string> readLogsFromEnd(int maxLines) const;
    std::vector<std::string> readLogsInRange(int startLine, int endLine) const;
    
    // 日志内容搜索
    bool containsInLogs(const std::string& searchText, 
                       int startLine = 0, 
                       int endLine = -1) const;

    // 刷新缓冲区
    void flush();

    // 获取当前日志路径
    std::string getCurrentLogPath() const;
    
    // 禁止拷贝和移动
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();
    
    // 获取当前时间字符串
    std::string getCurrentTime() const;
    
    // 获取日志级别字符串
    std::string getLevelString(LogLevel level) const;
    
    // 设置控制台颜色
    void setConsoleColor(LogLevel level) const;
    
    // 重置控制台颜色
    void resetConsoleColor() const;
    
    // 内部日志实现
    void logInternal(LogLevel level, const std::string& message);
    
    // 内部读取日志文件
    std::vector<std::string> readLogFileUnlocked() const;
    
    // 成员变量
    std::atomic<LogLevel> currentLevel_;
    std::atomic<bool> consoleOutput_;
    std::unique_ptr<std::ofstream> fileStream_;
    mutable std::mutex logMutex_;
    std::string logFilePath_;
};

}