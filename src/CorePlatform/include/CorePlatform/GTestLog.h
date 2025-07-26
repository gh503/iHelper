#pragma once
#include <gtest/gtest.h>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <chrono>

#if defined(_WIN32)
#include <Windows.h>
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

class TimestampedListener : public testing::EmptyTestEventListener {
public:
    explicit TimestampedListener(std::ostream* console_stream, std::ostream* file_stream = nullptr) 
        : console_stream_(console_stream), file_stream_(file_stream) {
        InitializeConsole();
        use_color_ = ShouldUseColor();
    }
    
    ~TimestampedListener() {
        #if defined(_WIN32)
        if (original_console_mode_ != 0) {
            SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), original_console_mode_);
        }
        #endif
    }
    
    void InitializeConsole() {
        #if defined(_WIN32)
        // 设置控制台输入输出为UTF-8编码
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        
        // 获取原始控制台模式以便恢复
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdout != INVALID_HANDLE_VALUE) {
            GetConsoleMode(hStdout, &original_console_mode_);
        }
        #endif
    }
    
    std::string TimestampPrefix() {
        // 获取当前时间（毫秒精度）
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = now_ms.time_since_epoch();
        auto value = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);
        std::time_t now_t = std::chrono::system_clock::to_time_t(now);
        
        // 线程安全的时间转换
        struct tm time_info;
        #if defined(_WIN32)
        localtime_s(&time_info, &now_t);
        #else
        localtime_r(&now_t, &time_info);
        #endif
        
        // 提取毫秒部分
        long milliseconds = value.count() % 1000;

        std::ostringstream oss;
        // 格式化输出：YYYY-MM-DD HH:MM:SS.sss
        oss << "[" << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << milliseconds << "]";
        return oss.str();
    }

    // 格式化耗时为 xh ymin zs mms
    std::string FormatDuration(long long milliseconds) {
        long long hours = milliseconds / (1000 * 60 * 60);
        milliseconds %= (1000 * 60 * 60);
        long long minutes = milliseconds / (1000 * 60);
        milliseconds %= (1000 * 60);
        long long seconds = milliseconds / 1000;
        milliseconds %= 1000;
        
        std::ostringstream oss;
        if (hours > 0) {
            oss << hours << "h ";
        }
        if (minutes > 0 || hours > 0) {
            oss << minutes << "min ";
        }
        if (seconds > 0 || minutes > 0 || hours > 0) {
            oss << seconds << "s ";
        }
        oss << milliseconds << "ms";
        return oss.str();
    }
    
    // ANSI 颜色代码
    const char* RESET_COLOR() const { return use_color_ ? "\033[0m" : ""; }
    const char* RED() const { return use_color_ ? "\033[1;31m" : ""; }
    const char* GREEN() const { return use_color_ ? "\033[1;32m" : ""; }
    const char* YELLOW() const { return use_color_ ? "\033[1;33m" : ""; }
    const char* BLUE() const { return use_color_ ? "\033[1;34m" : ""; }
    
    // 检查是否应该使用颜色
    bool ShouldUseColor() {
        const char* gtest_color = ::testing::GTEST_FLAG(color).c_str();
        
        // 检查环境变量
        if (const char* env_color = std::getenv("GTEST_COLOR")) {
            gtest_color = env_color;
        }
        
        // 检查是否强制启用
        if (strcmp(gtest_color, "yes") == 0 || strcmp(gtest_color, "true") == 0) {
            return true;
        }
        
        // 检查是否强制禁用
        if (strcmp(gtest_color, "no") == 0 || strcmp(gtest_color, "false") == 0) {
            return false;
        }
        
        // 自动检测：检查是否输出到终端
        #if defined(_WIN32)
        // Windows 检测和控制台设置
        if (console_stream_ == &std::cout) {
            HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hStdout != INVALID_HANDLE_VALUE) {
                DWORD mode;
                if (GetConsoleMode(hStdout, &mode)) {
                    // 启用虚拟终端处理
                    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    if (SetConsoleMode(hStdout, mode)) {
                        return true;
                    }
                }
            }
        }
        return false;
        #else
        // Linux/macOS 检测
        return ISATTY(FILENO(stdout)) != 0;
        #endif
    }
    
    void OnTestStart(const testing::TestInfo& test_info) override {
        test_start_time_ = std::chrono::steady_clock::now();
        if (console_stream_) {
            if (use_color_) {
                *console_stream_ << TimestampPrefix()
                    << YELLOW() << " run " << RESET_COLOR()
                    << test_info.test_suite_name() << "." << test_info.name()
                    << "\n";
            } else {
                *console_stream_ << TimestampPrefix() << " run " 
                    << test_info.test_suite_name() << "." << test_info.name() << "\n";
            }
        }

        if (file_stream_) {
            *file_stream_ << TimestampPrefix() << " run " 
                << test_info.test_suite_name() << "." << test_info.name() << "\n";
        }
    }
    
    void OnTestEnd(const testing::TestInfo& test_info) override {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - test_start_time_).count();
        const bool passed = test_info.result()->Passed();
        if (console_stream_) {
            if (use_color_) {
                *console_stream_ << TimestampPrefix()
                    << (passed ? GREEN() : RED()) << (passed ? " pass " : " fail ") << RESET_COLOR()
                    << test_info.test_suite_name() << "." << test_info.name()
                    << " (" << duration << " ms)" << "\n";
            } else {
                *console_stream_ << TimestampPrefix() << (passed ? " pass " : " fail ") 
                    << test_info.test_suite_name() << "." << test_info.name()
                    << " (" << duration << " ms)" << "\n";
            }
        }

        if (file_stream_) {
            *file_stream_ << TimestampPrefix() << (passed ? " pass " : " fail ") 
                << test_info.test_suite_name() << "." << test_info.name()
                    << " (" << duration << " ms)" << "\n";
        }
    }
    
    void OnTestPartResult(const testing::TestPartResult& result) override {
        if (result.failed()) {
            if (console_stream_) {
                if (use_color_) {
                    *console_stream_ << TimestampPrefix()
                        << RED() << " failed message info:" << "\n"
                        << result.file_name() << ":" << result.line_number() << "\n"
                        << result.message() << "\n" << RESET_COLOR();
                } else {
                    *console_stream_ << TimestampPrefix() << " failed message info: " << "\n"
                        << result.file_name() << ":" << result.line_number() << "\n"
                        << result.message() << "\n";
                }
            }
        
            if (file_stream_) {
                *file_stream_ << TimestampPrefix() << " failed message info: " << "\n"
                    << result.file_name() << ":" << result.line_number() << "\n"
                    << result.message() << "\n";
            }
        }
    }
    
    void OnTestProgramStart(const testing::UnitTest& /*unit_test*/) override {
        program_start_time_ = std::chrono::steady_clock::now();
        if (console_stream_) {
            if (use_color_) {
                *console_stream_ << TimestampPrefix() << BLUE() << " === Test Program Start ===\n" << RESET_COLOR();
            } else {
                *console_stream_ << TimestampPrefix() << " === Test Program Start ===\n";
            }
        }
        
        if (file_stream_) {
            *file_stream_ << TimestampPrefix() << " === Test Program Start ===\n";
        }
    }
    
    void OnTestProgramEnd(const testing::UnitTest& unit_test) override {
        auto end_time = std::chrono::steady_clock::now();
        auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - program_start_time_).count();
        std::string duration_str = FormatDuration(total_ms);

        if (console_stream_) {
            if (use_color_) {
                *console_stream_ << TimestampPrefix() << BLUE() << " === Test Program End ===\n\n" << RESET_COLOR()
                    << "Tests passed: " << GREEN() << unit_test.successful_test_count() << RESET_COLOR()
                    << ", failed: " << RED() << unit_test.failed_test_count() << RESET_COLOR() << "\n"
                    << "Time cost: " << YELLOW() << duration_str << RESET_COLOR() << "\n";
            } else {
                *console_stream_ << TimestampPrefix() << " === Test Program End ===\n\n"
                    << "Tests passed: " << unit_test.successful_test_count()
                    << ", failed: " << unit_test.failed_test_count() << "\n"
                    << "Time cost: " << duration_str << "\n";
            }
        }
        
        if (file_stream_) {
            *file_stream_ << TimestampPrefix() << " === Test Program End ===\n\n"
                << "Tests passed: " << unit_test.successful_test_count()
                << ", failed: " << unit_test.failed_test_count() << "\n"
                << "Time cost: " << duration_str << "\n";
        }
    }

private:
    std::ostream* console_stream_;
    std::ostream* file_stream_;
    bool use_color_ = false;
    // 时间记录
    std::chrono::steady_clock::time_point test_start_time_;
    std::chrono::steady_clock::time_point program_start_time_;
    
    #if defined(_WIN32)
    DWORD original_console_mode_ = 0;
    #endif
};