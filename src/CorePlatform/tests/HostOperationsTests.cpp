#include <gtest/gtest.h>
#include "CorePlatform/HostOperations.h"
#include "CorePlatformTest/TestUtils.h"
#include <thread>
#include <fstream>
#include <chrono>

using namespace CorePlatform;
using namespace CorePlatformTest;
using namespace std::chrono_literals;

// 测试进程操作
class ProcessOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::make_unique<TempDirectory>("ProcessTest");
    }

    void TearDown() override {
        tempDir.reset();
    }

    std::unique_ptr<TempDirectory> tempDir;
};

// 测试服务操作
class ServiceOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 确保服务不存在
        try {
            UninstallService(TEST_SERVICE_NAME);
        } catch (...) {
            // 忽略错误
        }
    }

    void TearDown() override {
        // 清理测试服务
        try {
            StopService(TEST_SERVICE_NAME);
        } catch (...) {
            // 忽略错误
        }
        
        try {
            UninstallService(TEST_SERVICE_NAME);
        } catch (...) {
            // 忽略错误
        }
    }

    const std::string TEST_SERVICE_NAME = "CorePlatformTestService";
};

// 测试驱动操作
class DriverOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 确保驱动不存在
        try {
            UninstallDriver(TEST_DRIVER_NAME);
        } catch (...) {
            // 忽略错误
        }
    }

    void TearDown() override {
        // 清理测试驱动
        try {
            UnloadDriver(TEST_DRIVER_NAME);
        } catch (...) {
            // 忽略错误
        }
        
        try {
            UninstallDriver(TEST_DRIVER_NAME);
        } catch (...) {
            // 忽略错误
        }
    }

    const std::string TEST_DRIVER_NAME = "CorePlatformTestDriver";
};

// 测试列出进程
TEST_F(ProcessOperationsTest, ListProcesses) {
    auto processes = ListProcesses();
    ASSERT_FALSE(processes.empty());
    
    bool foundCurrentProcess = false;
    for (const auto& proc : processes) {
        if (proc.pid == getpid()) {
            foundCurrentProcess = true;
            break;
        }
    }
    EXPECT_TRUE(foundCurrentProcess) << "Current process not found in process list";
}

// 测试启动和终止进程
TEST_F(ProcessOperationsTest, StartAndTerminateProcess) {
    // 创建测试脚本
    std::string scriptPath;
    std::string scriptContent;
    int expectedPid = 0;
    
    // 创建平台特定的测试脚本
#ifdef _WIN32
    scriptPath = tempDir->CreateFilePath("test_script.bat");
    scriptContent = "@echo off\n"
                    "echo Running test script\n"
                    "timeout /t 10 > nul\n"; // 等待10秒
#else
    scriptPath = tempDir->CreateFilePath("test_script.sh");
    scriptContent = "#!/bin/sh\n"
                    "echo \"Running test script\"\n"
                    "sleep 10\n"; // 等待10秒
#endif

    // 写入脚本文件
    {
        TempFile scriptFile(scriptPath);
        scriptFile.WriteContent(scriptContent);
    }
    
#ifdef __linux__
    // Linux 需要设置执行权限
    FileSystem::SetPermissions(scriptPath, FilePermissions::OWNER_EXECUTE | 
                                         FilePermissions::OWNER_READ | 
                                         FilePermissions::OWNER_WRITE);
#endif

    // 启动脚本
    StartProcess(scriptPath);
    
    // 查找新启动的进程
    bool found = false;
    int sleepPid = 0;
    auto processes = ListProcesses();
    for (const auto& proc : processes) {
        // 根据平台查找进程
#ifdef _WIN32
        if (proc.name.find("cmd.exe") != std::string::npos && 
            proc.commandLine.find(scriptPath) != std::string::npos) {
            sleepPid = proc.pid;
            found = true;
            break;
        }
#else
        if (proc.name.find("sleep") != std::string::npos || 
            proc.commandLine.find("sleep") != std::string::npos) {
            sleepPid = proc.pid;
            found = true;
            break;
        }
#endif
    }
    
    if (found) {
        // 终止进程
        EXPECT_NO_THROW(TerminateProcess(sleepPid)));
        
        // 等待进程结束
        std::this_thread::sleep_for(500ms);
        
        // 验证进程已终止
        EXPECT_THROW(GetProcessInfo(sleepPid), HostException);
    } else {
        GTEST_SKIP() << "Failed to find test process";
    }
}

// 测试获取进程信息
TEST_F(ProcessOperationsTest, GetProcessInfo) {
    int currentPid = getpid();
    auto info = GetProcessInfo(currentPid);
    
    EXPECT_EQ(info.pid, currentPid);
    EXPECT_FALSE(info.name.empty());
    EXPECT_FALSE(info.owner.empty());
    EXPECT_GT(info.memoryUsage, 0);
    EXPECT_GT(info.startTime, 0);
    EXPECT_FALSE(info.commandLine.empty());
}

// 测试安装和卸载服务
TEST_F(ServiceOperationsTest, InstallAndUninstallService) {
    // 获取测试程序路径
    std::string testProgram;
#ifdef _WIN32
    testProgram = "C:\\Windows\\System32\\cmd.exe";
#else
    testProgram = "/bin/echo";
#endif

    // 安装服务
    EXPECT_NO_THROW(InstallService(TEST_SERVICE_NAME, "CorePlatform Test Service", testProgram)));
    
    // 验证服务已安装
    EXPECT_NO_THROW({
        auto info = GetServiceInfo(TEST_SERVICE_NAME);
        EXPECT_EQ(info.name, TEST_SERVICE_NAME);
        EXPECT_EQ(info.displayName, "CorePlatform Test Service");
        EXPECT_EQ(info.binaryPath, testProgram);
    });
    
    // 卸载服务
    EXPECT_NO_THROW(UninstallService(TEST_SERVICE_NAME)));
    
    // 验证服务已卸载
    EXPECT_THROW(GetServiceInfo(TEST_SERVICE_NAME), HostException);
}

// 测试启动和停止服务
TEST_F(ServiceOperationsTest, StartAndStopService) {
    // 使用一个实际存在的服务进行测试
    std::string testService;
#ifdef _WIN32
    testService = "EventLog"; // Windows 事件日志服务
#elif __APPLE__
    testService = "com.apple.apsd"; // Apple Push 服务
#else
    testService = "cron"; // Linux cron 服务
#endif

    // 停止服务
    try {
        StopService(testService);
        std::this_thread::sleep_for(2s); // 等待服务停止
    } catch (...) {
        // 忽略停止错误
    }
    
    // 启动服务
    EXPECT_NO_THROW(StartService(testService)));
    
    // 验证服务状态
    EXPECT_NO_THROW({
        auto info = GetServiceInfo(testService);
        EXPECT_TRUE(info.status == ServiceStatus::Running || 
                   info.status == ServiceStatus::StartPending);
    });
    
    // 停止服务
    EXPECT_NO_THROW(StopService(testService)));
    
    // 验证服务状态
    EXPECT_NO_THROW({
        auto info = GetServiceInfo(testService);
        EXPECT_TRUE(info.status == ServiceStatus::Stopped || 
                   info.status == ServiceStatus::StopPending);
    });
}

// 测试列出服务
TEST_F(ServiceOperationsTest, ListServices) {
    auto services = ListServices();
    ASSERT_FALSE(services.empty());
    
    bool foundKnownService = false;
    for (const auto& service : services) {
        if (!service.name.empty()) {
            foundKnownService = true;
            break;
        }
    }
    EXPECT_TRUE(foundKnownService) << "No valid services found in service list";
}

// 测试安装和卸载驱动
TEST_F(DriverOperationsTest, InstallAndUninstallDriver) {
    // 获取测试驱动路径
    std::string testDriver;
#ifdef _WIN32
    testDriver = "C:\\Windows\\System32\\drivers\\null.sys"; // Windows 空设备驱动
#elif __linux__
    testDriver = "/lib/modules/$(uname -r)/kernel/drivers/char/nvram.ko"; // Linux NVRAM 驱动
#else
    // macOS 不支持驱动操作，跳过测试
    GTEST_SKIP() << "Driver operations not supported on macOS";
#endif

    // 安装驱动
    EXPECT_NO_THROW(InstallDriver(TEST_DRIVER_NAME, testDriver)));
    
    // 验证驱动已安装
    EXPECT_NO_THROW({
        auto info = GetDriverInfo(TEST_DRIVER_NAME);
        EXPECT_EQ(info.name, TEST_DRIVER_NAME);
        EXPECT_EQ(info.filePath, testDriver);
    });
    
    // 卸载驱动
    EXPECT_NO_THROW(UninstallDriver(TEST_DRIVER_NAME)));
    
    // 验证驱动已卸载
    EXPECT_THROW(GetDriverInfo(TEST_DRIVER_NAME), HostException);
}

// 测试加载和卸载驱动
TEST_F(DriverOperationsTest, LoadAndUnloadDriver) {
    // 使用一个实际存在的驱动进行测试
    std::string testDriver;
#ifdef _WIN32
    testDriver = "Null"; // Windows 空设备驱动
#elif __linux__
    testDriver = "nfs"; // Linux NFS 驱动
#else
    // macOS 不支持驱动操作，跳过测试
    GTEST_SKIP() << "Driver operations not supported on macOS";
#endif

    // 卸载驱动（确保初始状态）
    try {
        UnloadDriver(testDriver);
        std::this_thread::sleep_for(2s); // 等待卸载完成
    } catch (...) {
        // 忽略卸载错误
    }
    
    // 加载驱动
    EXPECT_NO_THROW(LoadDriver(testDriver)));
    
    // 验证驱动状态
    EXPECT_NO_THROW({
        auto info = GetDriverInfo(testDriver);
        EXPECT_EQ(info.status, DriverStatus::Running);
    });
    
    // 卸载驱动
    EXPECT_NO_THROW(UnloadDriver(testDriver)));
    
    // 验证驱动状态
    EXPECT_NO_THROW({
        auto info = GetDriverInfo(testDriver);
        EXPECT_EQ(info.status, DriverStatus::Stopped);
    });
}

// 测试获取驱动信息
TEST_F(DriverOperationsTest, GetDriverInfo) {
    // 使用一个已知驱动进行测试
    std::string testDriver;
#ifdef _WIN32
    testDriver = "Null"; // Windows 空设备驱动
#elif __linux__
    testDriver = "ext4"; // Linux EXT4 文件系统驱动
#else
    // macOS 不支持驱动操作，跳过测试
    GTEST_SKIP() << "Driver operations not supported on macOS";
#endif

    auto info = GetDriverInfo(testDriver);
    
    EXPECT_EQ(info.name, testDriver);
    EXPECT_FALSE(info.filePath.empty());
    EXPECT_TRUE(info.status == DriverStatus::Running || 
               info.status == DriverStatus::BootLoaded);
}