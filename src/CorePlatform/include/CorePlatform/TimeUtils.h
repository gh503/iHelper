#pragma once

#include <chrono>
#include <ctime>
#include <string>
#include <thread>
#include "CorePlatform/Export.h"

namespace CorePlatform {

class CORE_PLATFORM_API TimeUtils {
public:
    /**
     * 精确休眠等待指定毫秒数
     * @param milliseconds 要等待的毫秒数
     */
    static void sleep(int milliseconds);
    
    /**
     * 不休眠等待指定毫秒数（忙等待）
     * @param milliseconds 要等待的毫秒数
     * 
     * 注意：这会占用CPU资源，仅在需要精确延迟且时间很短时使用
     */
    static void busyWait(int milliseconds);
    
    /**
     * 获取当前时间戳（微秒级）
     * @return 自1970年1月1日以来的微秒数
     */
    static int64_t currentMicros();

    /**
     * 获取当前时间戳（毫秒级）
     * @return 自1970年1月1日以来的毫秒数
     */
    static int64_t currentMillis();
    
    /**
     * 获取当前时间戳（秒级）
     * @return 自1970年1月1日以来的秒数
     */
    static int64_t currentSeconds();
    
    /**
     * 获取当前日期字符串
     * @return 格式为 "YYYY-MM-DD" 的日期字符串
     */
    static std::string currentDate();
    
    /**
     * 获取当前时间字符串（精确到秒）
     * @return 格式为 "HH:MM:SS" 的时间字符串
     */
    static std::string currentTime();
    
    /**
     * 获取当前时间字符串（精确到毫秒）
     * @return 格式为 "HH:MM:SS.mmm" 的时间字符串
     */
    static std::string currentTimeMillis();
    
    /**
     * 获取当前日期时间字符串（精确到秒）
     * @return 格式为 "YYYY-MM-DD HH:MM:SS" 的日期时间字符串
     */
    static std::string currentDateTime();
    
    /**
     * 获取当前日期时间字符串（精确到毫秒）
     * @return 格式为 "YYYY-MM-DD HH:MM:SS.mmm" 的日期时间字符串
     */
    static std::string currentDateTimeMillis();
    
    /**
     * 测量代码块的执行时间
     * @tparam Func 可调用对象类型
     * @param func 要测量的函数
     * @param iterations 执行次数（默认为1）
     * @return 执行时间（毫秒）
     */
    template<typename Func>
    static double measureExecutionTime(Func func, int iterations = 1);
    
    /**
     * 高精度计时器（用于性能分析）
     */
    class Timer {
    public:
        Timer();
        
        /**
         * 启动计时器
         */
        void start();
        
        /**
         * 停止计时器
         */
        void stop();
        
        /**
         * 获取经过的时间（不停止计时器）
         * @return 经过的时间（毫秒）
         */
        double elapsedMilliseconds() const;

        /**
         * 获取经过的时间（不停止计时器）
         * @return 经过的时间（微秒）
         */
        double elapsedMicroseconds() const;
        
        /**
         * 重置计时器
         */
        void reset();
        
    private:
        using Clock = std::chrono::steady_clock; // 使用steady_clock避免系统时间变化
        using TimePoint = std::chrono::time_point<Clock>;
        using Duration = std::chrono::microseconds; // 使用整数微秒避免浮点精度问题
        
        TimePoint startTime;
        Duration totalDuration;
        bool isRunning;
    };

private:
    // 禁用实例化
    TimeUtils() = delete;
    ~TimeUtils() = delete;
};

// 模板函数实现
template<typename Func>
double TimeUtils::measureExecutionTime(Func func, int iterations) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        func();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

} // namespace CorePlatform