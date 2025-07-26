#include "CorePlatform/TimeUtils.h"
#include <chrono>
#include <ctime>
#include <thread>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#endif

namespace CorePlatform {

// ====================== 时间等待 ======================

void TimeUtils::sleep(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void TimeUtils::busyWait(int milliseconds) {
    auto start = std::chrono::high_resolution_clock::now();
    auto end = start + std::chrono::milliseconds(milliseconds);
    
    while (std::chrono::high_resolution_clock::now() < end) {
        // 空循环 - 忙等待
    }
}

// ====================== 时间戳获取 ======================
int64_t TimeUtils::currentMicros() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
}

int64_t TimeUtils::currentMillis() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

int64_t TimeUtils::currentSeconds() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
}

// ====================== 日期时间格式化 ======================

std::string TimeUtils::currentDate() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d");
    return ss.str();
}

std::string TimeUtils::currentTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&bt, "%H:%M:%S");
    return ss.str();
}

std::string TimeUtils::currentTimeMillis() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&bt, "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string TimeUtils::currentDateTime() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string TimeUtils::currentDateTimeMillis() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// ====================== 计时器实现 ======================
TimeUtils::Timer::Timer() 
    : totalDuration(0), isRunning(false) {}

void TimeUtils::Timer::start() {
    if (!isRunning) {
        startTime = Clock::now();
        isRunning = true;
    }
}

void TimeUtils::Timer::stop() {
    if (isRunning) {
        auto end = Clock::now();
        totalDuration += std::chrono::duration_cast<Duration>(end - startTime);
        isRunning = false;
    }
}

double TimeUtils::Timer::elapsedMilliseconds() const {
    Duration currentDuration = totalDuration;
    
    if (isRunning) {
        auto now = Clock::now();
        currentDuration += std::chrono::duration_cast<Duration>(now - startTime);
    }
    
    return static_cast<double>(currentDuration.count()) / 1000.0;
}

double TimeUtils::Timer::elapsedMicroseconds() const {
    Duration currentDuration = totalDuration;
    
    if (isRunning) {
        auto now = Clock::now();
        currentDuration += std::chrono::duration_cast<Duration>(now - startTime);
    }
    
    return static_cast<double>(currentDuration.count());
}

void TimeUtils::Timer::reset() {
    totalDuration = Duration(0);
    isRunning = false;
}

} // namespace CorePlatform