#include <gtest/gtest.h>
#include "CorePlatform/Process.h"
#include <iostream>

using namespace CorePlatform;

TEST(ProcessTest, ExecuteCommand) {
    // 测试同步执行
    auto result = Process::Execute("cmd.exe", {"/c", "echo", "Hello, CorePlatform!"});
    EXPECT_EQ(result.exitCode, 0);
    std::cerr << result.output << std::endl << result.error << std::endl;
    EXPECT_EQ(result.output, "Hello, CorePlatform!\n");
    EXPECT_TRUE(result.error.empty());
}

TEST(ProcessTest, ProcessLifecycle) {
    Process process;
    
    #ifdef _WIN32
        bool started = process.Start("cmd", {"/c", "echo Hello && timeout 1"});
    #else
        bool started = process.Start("sh", {"-c", "echo Hello; sleep 1"});
    #endif
    
    ASSERT_TRUE(started);
    EXPECT_TRUE(process.IsRunning());
    
    // 读取输出
    std::string output = process.ReadOutput();
    EXPECT_EQ(output, "Hello\n");
    
    // 等待进程结束
    EXPECT_TRUE(process.WaitForFinished(2000));
    EXPECT_FALSE(process.IsRunning());
    
    auto exitCode = process.ExitCode();
    EXPECT_TRUE(exitCode.has_value());
    EXPECT_EQ(exitCode.value(), 0);
}

TEST(ProcessTest, CurrentProcessInfo) {
    int64_t pid = Process::CurrentProcessId();
    EXPECT_GT(pid, 0);
    
    int64_t ppid = Process::ParentProcessId();
    EXPECT_GT(ppid, 0);
    
    std::string path = Process::CurrentProcessPath();
    EXPECT_FALSE(path.empty());
}