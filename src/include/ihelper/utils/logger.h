#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <ctime>

class Logger {
public:
    enum Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };
    
    static void init(Level min_level = INFO) {
        min_level_ = min_level;
    }
    
    template<typename... Args>
    static void debug(const std::string& format, Args&&... args) {
        log(DEBUG, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void info(const std::string& format, Args&&... args) {
        log(INFO, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void warning(const std::string& format, Args&&... args) {
        log(WARNING, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    static void error(const std::string& format, Args&&... args) {
        log(ERROR, format, std::forward<Args>(args)...);
    }
    
private:
    static Level min_level_;
    static std::mutex mutex_;
    
    template<typename... Args>
    static void log(Level level, const std::string& format, Args&&... args) {
        if (level < min_level_) return;
        
        std::string level_str;
        switch (level) {
            case DEBUG: level_str = "DEBUG"; break;
            case INFO: level_str = "INFO"; break;
            case WARNING: level_str = "WARNING"; break;
            case ERROR: level_str = "ERROR"; break;
        }
        
        // 手动实现简单的格式化
        std::string formatted;
        formatted.reserve(format.size() + 256);
        format_message(formatted, format, std::forward<Args>(args)...);
        
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        auto now_tm = *std::localtime(&now_time);
        
        char time_str[20];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &now_tm);
        
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "[" << time_str << "] [" << level_str << "] " << formatted << "\n";
    }
    
    // 基础格式化函数
    static void format_message(std::string& result, const std::string& format) {
        result = format;
    }
    
    template<typename T, typename... Args>
    static void format_message(std::string& result, const std::string& format, T&& value, Args&&... args) {
        size_t pos = format.find("{}");
        if (pos == std::string::npos) {
            result = format;
            return;
        }
        
        std::ostringstream oss;
        oss << value;
        std::string value_str = oss.str();
        
        std::string new_format = format.substr(0, pos) + value_str + format.substr(pos + 2);
        format_message(result, new_format, std::forward<Args>(args)...);
    }
};

Logger::Level Logger::min_level_ = Logger::INFO;
std::mutex Logger::mutex_;

#endif // LOGGER_H