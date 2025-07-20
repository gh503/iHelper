#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "PluginInterface.h"
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class PluginManager {
public:
    PluginManager();
    ~PluginManager();
    
    void loadPlugin(const std::string& path);
    void unloadPlugin(const std::string& name);
    void reloadPlugin(const std::string& name);
    void scanForPlugins(const std::string& directory);
    void startMonitoring();
    void stopMonitoring();
    
    std::vector<std::string> getLoadedPlugins() const;
    std::string getPlatformExtension() const;

    // 添加 UTF-8 到宽字符的转换函数
    #ifdef _WIN32
    static std::wstring utf8_to_wstring(const std::string& utf8_str);
    static std::string wstring_to_utf8(const std::wstring& wstr);
    #endif

private:
    struct PluginHandle {
        void* libraryHandle = nullptr;
        std::unique_ptr<IPlugin> instance;
        std::string name;
        std::filesystem::path path;
        std::filesystem::file_time_type lastWriteTime;
    };
    
    std::unordered_map<std::string, PluginHandle> plugins;
    mutable std::mutex pluginsMutex;
    std::atomic<bool> monitoring{false};
    std::thread monitorThread;
    
    void* loadLibrary(const std::string& path);
    void unloadLibrary(void* handle);
    void* getSymbol(void* handle, const char* symbol);
    void monitoringThreadFunc();
};

#endif // PLUGIN_MANAGER_H