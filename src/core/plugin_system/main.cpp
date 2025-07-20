#include "PluginSystemAPI.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>

std::atomic<bool> running{true};

void signalHandler(int) {
    running = false;
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    auto& pluginSystem = PluginSystem::getInstance();
    
    // 示例：加载初始插件
    pluginSystem.scanDirectory("plugins");
    
    // 启动监控
    pluginSystem.startMonitoring();
    
    // 在单独线程中运行命令行接口
    std::thread cliThread([&]() {
        pluginSystem.runCommandLineInterface();
    });
    
    std::cout << "Main system running. Type 'plugin> exit' to stop command line." << std::endl;
    
    // 主循环
    while (running) {
        // 主程序逻辑
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // 调用插件功能
        auto plugins = pluginSystem.getLoadedPlugins();
        if (!plugins.empty()) {
            std::cout << "Active plugins: ";
            for (const auto& name : plugins) {
                std::cout << name << " ";
            }
            std::cout << std::endl;
        }
    }
    
    // 清理
    pluginSystem.stopMonitoring();
    if (cliThread.joinable()) {
        cliThread.join();
    }
    
    std::cout << "Shutting down..." << std::endl;
    return 0;
}