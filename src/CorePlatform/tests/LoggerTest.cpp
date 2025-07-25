#include <gtest/gtest.h>
#include "CorePlatform/Logger.h"
#include "TestUtils.h"
#include <thread>
#include <future>
#include <vector>
#include <regex>

using namespace CorePlatformTest;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 重置Logger状态
        CorePlatform::Logger::getInstance().setLevel(CorePlatform::LogLevel::INFO);
        CorePlatform::Logger::getInstance().setConsoleOutput(false);
        
        // 创建临时日志文件
        tempDir = std::make_unique<TempDirectory>("LoggerTest");
        logFilePath = tempDir->CreateFilePath("test.log");
        ASSERT_TRUE(CorePlatform::Logger::getInstance().setLogFile(logFilePath));
    }
    
    void TearDown() override {
        // 清理临时目录
        tempDir.reset();
    }
    
    std::vector<std::string> ReadLogFileDirectly() {
        std::ifstream file(logFilePath);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        return lines;
    }
    
    std::unique_ptr<TempDirectory> tempDir;
    std::string logFilePath;
};

// 测试单例模式
TEST_F(LoggerTest, SingletonPattern) {
    CorePlatform::Logger& logger1 = CorePlatform::Logger::getInstance();
    CorePlatform::Logger& logger2 = CorePlatform::Logger::getInstance();
    ASSERT_EQ(&logger1, &logger2);
    
    // 测试拷贝构造删除
    EXPECT_TRUE(std::is_copy_constructible<CorePlatform::Logger>::value == false);
    // 测试赋值操作删除
    EXPECT_TRUE(std::is_copy_assignable<CorePlatform::Logger>::value == false);
}

// 测试日志级别设置和过滤
TEST_F(LoggerTest, LogLevelFiltering) {
    CorePlatform::Logger& logger = CorePlatform::Logger::getInstance();
    
    logger.setLevel(CorePlatform::LogLevel::WARN);
    logger.trace("This trace should NOT appear");
    logger.debug("This debug should NOT appear");
    logger.info("This info should NOT appear");
    logger.warn("This warn should appear");
    logger.error("This error should appear");
    
    auto logs = logger.readAllLogs();
    VerifyLogEntry(logs, CorePlatform::LogLevel::TRACE, "NOT appear", 0);
    VerifyLogEntry(logs, CorePlatform::LogLevel::DEBUG, "NOT appear", 0);
    VerifyLogEntry(logs, CorePlatform::LogLevel::INFO, "NOT appear", 0);
    VerifyLogEntry(logs, CorePlatform::LogLevel::WARN, "should appear");
    VerifyLogEntry(logs, CorePlatform::LogLevel::ERR, "should appear");
}

// 测试控制台输出
TEST_F(LoggerTest, ConsoleOutput) {
    CorePlatform::Logger& logger = CorePlatform::Logger::getInstance();
    logger.setLevel(CorePlatform::LogLevel::TRACE);
    
    logger.trace("Trace convenience");
    logger.debug("Debug convenience");
    logger.info("Info convenience");
    logger.warn("Warn convenience");
    logger.error("Error convenience");
    logger.fatal("Fatal convenience");
    
    auto logs = logger.readAllLogs();
    VerifyLogEntry(logs, CorePlatform::LogLevel::TRACE, "Trace convenience");
    VerifyLogEntry(logs, CorePlatform::LogLevel::DEBUG, "Debug convenience");
    VerifyLogEntry(logs, CorePlatform::LogLevel::INFO, "Info convenience");
    VerifyLogEntry(logs, CorePlatform::LogLevel::WARN, "Warn convenience");
    VerifyLogEntry(logs, CorePlatform::LogLevel::ERR, "Error convenience");
    VerifyLogEntry(logs, CorePlatform::LogLevel::FATAL, "Fatal convenience");
}

// 测试日志读取功能
TEST_F(LoggerTest, LogReadingFunctions) {
    CorePlatform::Logger& logger = CorePlatform::Logger::getInstance();
    
    // 生成50条测试日志
    for (int i = 1; i <= 50; i++) {
        logger.info("Log entry " + std::to_string(i));
    }
    
    // 测试读取全部日志
    auto allLogs = logger.readAllLogs();
    ASSERT_EQ(allLogs.size(), 50);
    VerifyLogEntry(allLogs, CorePlatform::LogLevel::INFO, "Log entry 25");
    
    // 测试从开头读取
    auto first10 = logger.readLogsFromBeginning(10);
    ASSERT_EQ(first10.size(), 10);
    VerifyLogEntry(first10, CorePlatform::LogLevel::INFO, "Log entry 1", 2);
    VerifyLogEntry(first10, CorePlatform::LogLevel::INFO, "Log entry 10");
    VerifyLogEntry(first10, CorePlatform::LogLevel::INFO, "Log entry 11", 0);
    
    // 测试从末尾读取
    auto last10 = logger.readLogsFromEnd(10);
    ASSERT_EQ(last10.size(), 10);
    VerifyLogEntry(last10, CorePlatform::LogLevel::INFO, "Log entry 50");
    VerifyLogEntry(last10, CorePlatform::LogLevel::INFO, "Log entry 41");
    VerifyLogEntry(last10, CorePlatform::LogLevel::INFO, "Log entry 40", 0);
    
    // 测试范围读取
    auto rangeLogs = logger.readLogsInRange(20, 30);
    ASSERT_EQ(rangeLogs.size(), 11); // 包含第20条和第30条
    VerifyLogEntry(rangeLogs, CorePlatform::LogLevel::INFO, "Log entry 20");
    VerifyLogEntry(rangeLogs, CorePlatform::LogLevel::INFO, "Log entry 25");
    VerifyLogEntry(rangeLogs, CorePlatform::LogLevel::INFO, "Log entry 30");
    VerifyLogEntry(rangeLogs, CorePlatform::LogLevel::INFO, "Log entry 19", 0);
    VerifyLogEntry(rangeLogs, CorePlatform::LogLevel::INFO, "Log entry 31", 0);
}

// 测试日志搜索功能
TEST_F(LoggerTest, LogSearchFunction) {
    CorePlatform::Logger& logger = CorePlatform::Logger::getInstance();
    
    logger.info("Search test: apple");
    logger.warn("Search test: banana");
    logger.error("Search test: apple and banana");
    logger.info("Unrelated log entry");
    
    // 测试全文搜索
    ASSERT_TRUE(logger.containsInLogs("apple"));
    ASSERT_TRUE(logger.containsInLogs("banana"));
    ASSERT_FALSE(logger.containsInLogs("orange"));
    
    // 测试范围搜索
    ASSERT_TRUE(logger.containsInLogs("apple", 0, 1)); // 第0-1行
    ASSERT_FALSE(logger.containsInLogs("banana", 0, 1));
    ASSERT_TRUE(logger.containsInLogs("banana", 1, 2));
    
    // 测试负数索引（表示倒数）
    ASSERT_TRUE(logger.containsInLogs("apple", 0, -2)); // 排除最后一行
    ASSERT_FALSE(logger.containsInLogs("Unrelated", 0, -2));
}

// 测试多线程安全
TEST_F(LoggerTest, ThreadSafety) {
    CorePlatform::Logger& logger = CorePlatform::Logger::getInstance();
    logger.setLevel(CorePlatform::LogLevel::INFO);
    
    constexpr int THREAD_COUNT = 10;
    constexpr int LOGS_PER_THREAD = 100;
    
    auto logTask = [&](int threadId) {
        for (int i = 0; i < LOGS_PER_THREAD; i++) {
            logger.info("Thread " + std::to_string(threadId) + 
                       " log " + std::to_string(i));
        }
    };
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < THREAD_COUNT; i++) {
        futures.push_back(std::async(std::launch::async, logTask, i));
    }
    
    // 等待所有线程完成
    for (auto& f : futures) {
        f.get();
    }
    
    // 验证日志总数
    auto allLogs = logger.readAllLogs();
    ASSERT_EQ(allLogs.size(), THREAD_COUNT * LOGS_PER_THREAD);
    
    // 验证每个线程的日志都存在
    for (int i = 0; i < THREAD_COUNT; i++) {
        bool found = false;
        for (const auto& log : allLogs) {
            if (log.find("Thread " + std::to_string(i)) != std::string::npos) {
                found = true;
                break;
            }
        }
        ASSERT_TRUE(found) << "Logs from thread " << i << " not found";
    }
}

// 测试日志格式
TEST_F(LoggerTest, LogFormat) {
    CorePlatform::Logger& logger = CorePlatform::Logger::getInstance();
    logger.info("Format test message");
    
    auto logs = logger.readAllLogs();
    ASSERT_FALSE(logs.empty());
    
    // 使用正则表达式验证日志格式
    std::regex logPattern(R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\] \[INFO\].+)");
    ASSERT_TRUE(std::regex_match(logs[0], logPattern)) 
        << "Log format mismatch: " << logs[0];
}

// 测试日志文件切换
TEST_F(LoggerTest, LogFileSwitching) {
    CorePlatform::Logger& logger = CorePlatform::Logger::getInstance();
    
    // 在第一个文件写日志
    logger.info("First file log");
    
    // 创建第二个临时文件
    std::string newLogPath = tempDir->CreateFilePath("new.log");
    ASSERT_TRUE(logger.setLogFile(newLogPath));
    
    // 在第二个文件写日志
    logger.info("Second file log");

    // 验证第一个文件内容
    auto firstLogs = ReadLogFileDirectly();
    VerifyLogEntry(firstLogs, CorePlatform::LogLevel::INFO, "First file log");
    VerifyLogEntry(firstLogs, CorePlatform::LogLevel::INFO, "Second file log", 0);
    
    // 验证第二个文件内容 - 使用行分割
    auto secondLogContent = CorePlatform::FileSystem::ReadTextFile(newLogPath);
    auto secondLogLines = SplitIntoLines(secondLogContent);
    ASSERT_FALSE(secondLogLines.empty());
    VerifyLogEntry(secondLogLines, CorePlatform::LogLevel::INFO, "Second file log");
    VerifyLogEntry(secondLogLines, CorePlatform::LogLevel::INFO, "First file log", 0);
}

// 测试大日志处理
TEST_F(LoggerTest, LargeLogHandling) {
    CorePlatform::Logger& logger = CorePlatform::Logger::getInstance();
    
    // 生成10KB的日志消息
    std::string largeMsg(10*1024, 'X');
    logger.info(largeMsg);
    
    auto logs = logger.readAllLogs();
    ASSERT_EQ(logs.size(), 1);
    ASSERT_GT(logs[0].size(), 10*1024);
    ASSERT_TRUE(logs[0].find(largeMsg) != std::string::npos);
}