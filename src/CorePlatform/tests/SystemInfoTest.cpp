#include "CorePlatform/SystemInfo.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <regex>

using namespace CorePlatform;
using namespace std::chrono_literals;

TEST(SystemInfoTest, OSVersion) {
    auto version = SystemInfo::GetOSVersion();
    
    EXPECT_FALSE(version.name.empty());
    EXPECT_FALSE(version.version.empty());
    EXPECT_FALSE(version.build.empty());
    EXPECT_FALSE(version.architecture.empty());
    
    std::cout << "OS Name: " << version.name << std::endl;
    std::cout << "Version: " << version.version << std::endl;
    std::cout << "Build: " << version.build << std::endl;
    std::cout << "Architecture: " << version.architecture << std::endl;
}

TEST(SystemInfoTest, MemoryInfo) {
    auto memInfo = SystemInfo::GetMemoryInfo();
    
    EXPECT_GT(memInfo.totalPhysical, 0);
    EXPECT_GT(memInfo.availablePhysical, 0);
    EXPECT_LE(memInfo.availablePhysical, memInfo.totalPhysical);
    
    #ifdef _WIN32
    EXPECT_GT(memInfo.totalVirtual, 0);
    EXPECT_GT(memInfo.availableVirtual, 0);
    #endif
    
    std::cout << "Total Physical: " << memInfo.totalPhysical / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Available Physical: " << memInfo.availablePhysical / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Total Virtual: " << memInfo.totalVirtual / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Available Virtual: " << memInfo.availableVirtual / (1024 * 1024) << " MB" << std::endl;
}

TEST(SystemInfoTest, CPUInfo) {
    auto cpuInfo = SystemInfo::GetCPUInfo();
    
    EXPECT_FALSE(cpuInfo.vendor.empty());
    EXPECT_FALSE(cpuInfo.brand.empty());
    EXPECT_GT(cpuInfo.cores, 0);
    EXPECT_GT(cpuInfo.threads, 0);
    EXPECT_GT(cpuInfo.clockSpeed, 0.1);
    
    std::cout << "CPU Vendor: " << cpuInfo.vendor << std::endl;
    std::cout << "CPU Brand: " << cpuInfo.brand << std::endl;
    std::cout << "Cores: " << cpuInfo.cores << std::endl;
    std::cout << "Threads: " << cpuInfo.threads << std::endl;
    std::cout << "Clock Speed: " << cpuInfo.clockSpeed << " GHz" << std::endl;
}

TEST(SystemInfoTest, TimeInfo) {
    auto timeInfo = SystemInfo::GetSystemTimeInfo();
    auto bootTime = SystemInfo::GetBootTime();
    auto uptime = SystemInfo::GetUptime();
    
    EXPECT_GT(timeInfo.bootTime.time_since_epoch().count(), 0);
    EXPECT_GT(timeInfo.currentTime.time_since_epoch().count(), 0);
    EXPECT_GT(uptime.count(), 0);
    
    // 启动时间应该在当前时间之前
    EXPECT_LT(bootTime, timeInfo.currentTime);
    
    // 运行时间应该小于启动时间到当前时间的差值
    auto calculatedUptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        timeInfo.currentTime - bootTime);
    EXPECT_NEAR(uptime.count(), calculatedUptime.count(), 1000); // 允许1秒误差
    
    std::time_t bootTimeT = std::chrono::system_clock::to_time_t(bootTime);
    std::cout << "Boot Time: " << std::ctime(&bootTimeT);
    std::cout << "Uptime: " << uptime.count() / 1000 << " seconds" << std::endl;
}

TEST(SystemInfoTest, UserInfo) {
    auto username = SystemInfo::GetUsername();
    bool isAdmin = SystemInfo::IsAdmin();
    
    EXPECT_FALSE(username.empty());
    
    std::cout << "Username: " << username << std::endl;
    std::cout << "Is Admin: " << (isAdmin ? "Yes" : "No") << std::endl;
}

TEST(SystemInfoTest, DiskSpace) {
    // 获取临时目录
    std::filesystem::path tempDir = std::filesystem::temp_directory_path();
    auto diskInfo = SystemInfo::GetDiskSpace(tempDir.string());
    
    EXPECT_GT(diskInfo.totalSpace, 0);
    EXPECT_GT(diskInfo.availableSpace, 0);
    EXPECT_LE(diskInfo.availableSpace, diskInfo.totalSpace);
    
    std::cout << "Disk Path: " << tempDir << std::endl;
    std::cout << "Total Space: " << diskInfo.totalSpace / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Free Space: " << diskInfo.freeSpace / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Available Space: " << diskInfo.availableSpace / (1024 * 1024) << " MB" << std::endl;
}

TEST(SystemInfoTest, MountPoints) {
    auto mountPoints = SystemInfo::GetMountPoints();
    
    EXPECT_FALSE(mountPoints.empty());
    
    std::cout << "Mount Points:" << std::endl;
    for (const auto& mp : mountPoints) {
        auto diskInfo = SystemInfo::GetDiskSpace(mp);
        std::cout << "  " << mp << " - " 
                  << diskInfo.availableSpace / (1024 * 1024) << " MB free of "
                  << diskInfo.totalSpace / (1024 * 1024) << " MB" << std::endl;
    }
}
TEST(SystemInfoTest, OSVersion32BitCompatibility) {
    auto version = SystemInfo::GetOSVersion();
    
    // 验证所有字段都有有效值
    EXPECT_FALSE(version.name.empty());
    EXPECT_FALSE(version.version.empty());
    EXPECT_FALSE(version.build.empty());
    EXPECT_FALSE(version.architecture.empty());
    
    // 验证版本号格式
    std::regex versionRegex("\\d+\\.\\d+\\.\\d+");
    EXPECT_TRUE(std::regex_match(version.version, versionRegex));
    
    // 验证构建号格式
    std::regex buildRegex("\\d+");
    EXPECT_TRUE(std::regex_match(version.build, buildRegex));
    
    // 验证架构是已知值
    std::vector<std::string> validArch = {"x86", "x86_64", "ARM", "ARM64"};
    bool found = false;
    for (const auto& arch : validArch) {
        if (version.architecture == arch) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Unknown architecture: " << version.architecture;
}

TEST(SystemInfoTest, CPUInfo32BitCompatibility) {
    auto cpuInfo = SystemInfo::GetCPUInfo();
    
    // 验证所有字段都有有效值
    EXPECT_FALSE(cpuInfo.vendor.empty());
    EXPECT_FALSE(cpuInfo.brand.empty());
    EXPECT_GT(cpuInfo.cores, 0);
    EXPECT_GT(cpuInfo.threads, 0);
    EXPECT_GT(cpuInfo.clockSpeed, 0.1);
    
    // 验证核心数合理
    EXPECT_LE(cpuInfo.cores, 128);
    
    // 验证线程数合理
    EXPECT_LE(cpuInfo.threads, 256);
    
    // 验证主频在合理范围内
    EXPECT_GT(cpuInfo.clockSpeed, 0.5);
    EXPECT_LT(cpuInfo.clockSpeed, 10.0);
}

TEST(SystemInfoTest, Performance) {
    // 测试多次调用的性能
    const int iterations = 100;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        SystemInfo::GetMemoryInfo();
        SystemInfo::GetCPUInfo();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Performed " << iterations * 2 << " info queries in " 
              << duration.count() << " ms" << std::endl;
}