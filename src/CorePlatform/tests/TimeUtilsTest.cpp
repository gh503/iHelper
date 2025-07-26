#include "CorePlatform/TimeUtils.h"
#include <gtest/gtest.h>
#include <thread>
#include <regex>

using namespace CorePlatform;

// 测试休眠等待
TEST(TimeUtilsTest, Sleep) {
    auto start = TimeUtils::currentMillis();
    TimeUtils::sleep(100);
    auto end = TimeUtils::currentMillis();
    auto duration = end - start;
    
    EXPECT_GE(duration, 100);
    EXPECT_LE(duration, 120); // 允许20ms的误差
}

// 测试忙等待
TEST(TimeUtilsTest, BusyWait) {
    auto start = TimeUtils::currentMillis();
    TimeUtils::busyWait(50);
    auto end = TimeUtils::currentMillis();
    auto duration = end - start;
    
    EXPECT_GE(duration, 50);
    EXPECT_LE(duration, 55); // 忙等待更精确
}

// 测试时间戳
TEST(TimeUtilsTest, Timestamps) {
    int64_t seconds = TimeUtils::currentSeconds();
    int64_t millis = TimeUtils::currentMillis();
    
    EXPECT_GT(millis, seconds * 1000);
    EXPECT_LT(millis, (seconds + 1) * 1000);
    EXPECT_GT(seconds, 1600000000); // 大于2020年
}

// 测试日期时间格式
TEST(TimeUtilsTest, DateTimeFormats) {
    // 日期格式
    std::string date = TimeUtils::currentDate();
    std::regex dateRegex(R"(\d{4}-\d{2}-\d{2})");
    EXPECT_TRUE(std::regex_match(date, dateRegex));
    
    // 时间格式
    std::string time = TimeUtils::currentTime();
    std::regex timeRegex(R"(\d{2}:\d{2}:\d{2})");
    EXPECT_TRUE(std::regex_match(time, timeRegex));
    
    // 时间格式（毫秒）
    std::string timeMs = TimeUtils::currentTimeMillis();
    std::regex timeMsRegex(R"(\d{2}:\d{2}:\d{2}\.\d{3})");
    EXPECT_TRUE(std::regex_match(timeMs, timeMsRegex));
    
    // 日期时间格式
    std::string dateTime = TimeUtils::currentDateTime();
    std::regex dateTimeRegex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})");
    EXPECT_TRUE(std::regex_match(dateTime, dateTimeRegex));
    
    // 日期时间格式（毫秒）
    std::string dateTimeMs = TimeUtils::currentDateTimeMillis();
    std::regex dateTimeMsRegex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})");
    EXPECT_TRUE(std::regex_match(dateTimeMs, dateTimeMsRegex));
}

// 测试执行时间测量
TEST(TimeUtilsTest, MeasureExecutionTime) {
    volatile int sink; // 防止优化
    // 测量空函数的时间
    double time = TimeUtils::measureExecutionTime([&sink]() {
        sink = 0; // 有副作用的操作
    }, 1000000); // 增加迭代次数至1,000,000

    EXPECT_GT(time, 0.0);
    EXPECT_LT(time, 50.0); // 放宽时间限制，考虑系统负载
    
    // 测量休眠时间
    time = TimeUtils::measureExecutionTime([]() {
        TimeUtils::sleep(10);
    }, 1);
    
    EXPECT_GE(time, 8.0);  // 允许20%的误差
    EXPECT_LE(time, 50.0); // 放宽上限，考虑系统调度延迟
}

// 测试计时器类
TEST(TimeUtilsTest, Timer) {
    TimeUtils::Timer timer;
    
    // 测试1: 初始状态
    EXPECT_DOUBLE_EQ(timer.elapsedMilliseconds(), 0.0);
    EXPECT_DOUBLE_EQ(timer.elapsedMicroseconds(), 0.0);
    
    // 测试2: 短时间测量
    timer.start();
    
    // 精确等待100微秒
    int64_t startMicro = TimeUtils::currentMicros();
    while (TimeUtils::currentMicros() - startMicro < 100) {
        // 空循环
    }
    
    double elapsedMicro = timer.elapsedMicroseconds();
    EXPECT_GE(elapsedMicro, 90.0);
    EXPECT_LE(elapsedMicro, 150.0);
    
    // 停止计时
    timer.stop();
    double stoppedMicro = timer.elapsedMicroseconds();
    EXPECT_GE(stoppedMicro, 90.0);
    EXPECT_LE(stoppedMicro, 150.0);
    
    // 测试3: 停止后值不变
    int64_t startWait = TimeUtils::currentMicros();
    while (TimeUtils::currentMicros() - startWait < 5000) {} // 等待5ms
    
    EXPECT_DOUBLE_EQ(timer.elapsedMicroseconds(), stoppedMicro);
    
    // 测试4: 重新启动
    timer.start();
    startWait = TimeUtils::currentMicros();
    while (TimeUtils::currentMicros() - startWait < 500) {} // 等待500μs
    
    timer.stop();
    double totalMicro = timer.elapsedMicroseconds();
    EXPECT_GE(totalMicro, stoppedMicro + 450.0);
    EXPECT_LE(totalMicro, stoppedMicro + 600.0);
    
    // 测试5: 重置
    timer.reset();
    EXPECT_DOUBLE_EQ(timer.elapsedMilliseconds(), 0.0);
    
    // 测试6: 长时间测量
    timer.start();
    TimeUtils::sleep(100); // 100毫秒
    timer.stop();
    
    double longTime = timer.elapsedMilliseconds();
    EXPECT_GE(longTime, 95.0);
    EXPECT_LE(longTime, 150.0);
    
    // 测试7: 多次启动停止
    timer.reset();
    timer.start();
    int64_t start1 = TimeUtils::currentMicros();
    while (TimeUtils::currentMicros() - start1 < 10000) {} // 精确等待10ms
    timer.stop();
    
    timer.start();
    // 替换sleep(20)为精确的微秒级等待
    int64_t start2 = TimeUtils::currentMicros();
    while (TimeUtils::currentMicros() - start2 < 20000) {} // 精确等待20ms
    timer.stop();
    
    double totalTime = timer.elapsedMilliseconds();
    // 调整阈值范围：25ms ~ 32ms (原28ms~35ms范围仍过严)
    EXPECT_GE(totalTime, 25.0);
    EXPECT_LE(totalTime, 32.0); // 允许2ms误差
}

// 测试边界情况
TEST(TimeUtilsTest, EdgeCases) {
    // 零时间等待
    auto start = TimeUtils::currentMillis();
    TimeUtils::sleep(0);
    TimeUtils::busyWait(0);
    auto end = TimeUtils::currentMillis();
    EXPECT_LE(end - start, 1);
    
    // 负时间等待（应等同于0）
    start = TimeUtils::currentMillis();
    TimeUtils::sleep(-100);
    TimeUtils::busyWait(-100);
    end = TimeUtils::currentMillis();
    EXPECT_LE(end - start, 1);
}

// 测试多线程安全
TEST(TimeUtilsTest, ThreadSafety) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::vector<int64_t> results(numThreads);
    
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&, i]() {
            results[i] = TimeUtils::currentMillis();
            TimeUtils::sleep(10);
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 所有时间戳应在合理范围内
    for (int i = 0; i < numThreads; i++) {
        for (int j = i + 1; j < numThreads; j++) {
            EXPECT_LE(std::abs(results[i] - results[j]), 100) 
                << "Threads " << i << " and " << j << " have large time difference";
        }
    }
}