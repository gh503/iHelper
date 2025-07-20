#ifndef PLUGIN_SYSTEM_API_H
#define PLUGIN_SYSTEM_API_H

#include <string>
#include <vector>
#include <memory>

class PluginSystem {
public:
    static PluginSystem& getInstance();
    
    // 禁止复制
    PluginSystem(const PluginSystem&) = delete;
    PluginSystem& operator=(const PluginSystem&) = delete;
    
    // 插件管理API
    void loadPlugin(const std::string& path);
    void unloadPlugin(const std::string& name);
    void reloadPlugin(const std::string& name);
    void scanDirectory(const std::string& directory);
    void startMonitoring();
    void stopMonitoring();
    
    // 获取信息
    std::vector<std::string> getLoadedPlugins() const;
    std::string getPlatformExtension() const;
    
    // 命令行接口
    void executeCommand(const std::string& command);
    void runCommandLineInterface();

private:
    PluginSystem();
    ~PluginSystem();
    
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif // PLUGIN_SYSTEM_API_H