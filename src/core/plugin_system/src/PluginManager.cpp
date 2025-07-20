#include "PluginManager.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <cstdint>
#include <locale>
#include <codecvt>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace std::chrono_literals;

// Windows 下的 UTF-8 到宽字符转换
#ifdef _WIN32
std::wstring PluginManager::utf8_to_wstring(const std::string& utf8_str) {
    if (utf8_str.empty()) return L"";
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), (int)utf8_str.size(), nullptr, 0);
    if (size_needed <= 0) {
        throw std::runtime_error("MultiByteToWideChar failed: " + std::to_string(GetLastError()));
    }
    
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), (int)utf8_str.size(), &wstr[0], size_needed);
    return wstr;
}

std::string PluginManager::wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        throw std::runtime_error("WideCharToMultiByte failed: " + std::to_string(GetLastError()));
    }
    
    std::string utf8_str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &utf8_str[0], size_needed, nullptr, nullptr);
    return utf8_str;
}
#endif

PluginManager::PluginManager() {}

PluginManager::~PluginManager() {
    stopMonitoring();
    std::lock_guard<std::mutex> lock(pluginsMutex);
    for (auto& [name, handle] : plugins) {
        if (handle.instance) {
            handle.instance->shutdown();
            handle.instance.reset();
        }
        if (handle.libraryHandle) {
            unloadLibrary(handle.libraryHandle);
        }
    }
}

void* PluginManager::loadLibrary(const std::string& path) {
#ifdef _WIN32
    // 使用宽字符版本处理中文路径
    std::wstring wpath = utf8_to_wstring(path);
    return LoadLibraryW(wpath.c_str());
#else
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
}

void PluginManager::unloadLibrary(void* handle) {
    if (!handle) return;
    
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* PluginManager::getSymbol(void* handle, const char* symbol) {
    if (!handle) return nullptr;
    
#ifdef _WIN32
    FARPROC func = GetProcAddress(static_cast<HMODULE>(handle), symbol);
    return reinterpret_cast<void*>(static_cast<intptr_t>(reinterpret_cast<intptr_t>(func)));
#else
    return dlsym(handle, symbol);
#endif
}

std::string PluginManager::getPlatformExtension() const {
#ifdef _WIN32
    return ".dll";
#elif __APPLE__
    return ".dylib";
#else
    return ".so";
#endif
}

void PluginManager::loadPlugin(const std::string& path) {
    namespace fs = std::filesystem;
    
    // 检查文件是否存在（支持中文路径）
#ifdef _WIN32
    std::wstring wpath = utf8_to_wstring(path);
    if (!fs::exists(wpath)) {
#else
    if (!fs::exists(path)) {
#endif
        throw std::runtime_error("Plugin not found: " + path);
    }
    
    std::lock_guard<std::mutex> lock(pluginsMutex);
    
    void* lib = loadLibrary(path);
    if (!lib) {
#ifdef _WIN32
        DWORD error = GetLastError();
        throw std::runtime_error("LoadLibrary failed: " + std::to_string(error));
#else
        throw std::runtime_error("dlopen failed: " + std::string(dlerror()));
#endif
    }
    
    auto createFunc = reinterpret_cast<CreatePluginFunc>(getSymbol(lib, "createPlugin"));
    auto destroyFunc = reinterpret_cast<DestroyPluginFunc>(getSymbol(lib, "destroyPlugin"));
    
    if (!createFunc || !destroyFunc) {
        unloadLibrary(lib);
        throw std::runtime_error("Invalid plugin: missing entry functions");
    }
    
    std::unique_ptr<IPlugin> plugin(createFunc());
    std::string name = plugin->getName();
    
    if (plugins.find(name) != plugins.end()) {
        unloadPlugin(name);
    }
    
    plugin->initialize();
    
#ifdef _WIN32
    auto lastWriteTime = fs::last_write_time(wpath);
#else
    auto lastWriteTime = fs::last_write_time(path);
#endif
    
    plugins[name] = PluginHandle{
        lib,
        std::move(plugin),
        name,
        path,
        lastWriteTime
    };
    
    std::cout << "Loaded plugin: " << name << std::endl;
}

void PluginManager::unloadPlugin(const std::string& name) {
    std::lock_guard<std::mutex> lock(pluginsMutex);
    
    auto it = plugins.find(name);
    if (it == plugins.end()) {
        throw std::runtime_error("Plugin not found: " + name);
    }
    
    auto& handle = it->second;
    handle.instance->shutdown();
    handle.instance.reset();
    
    auto destroyFunc = reinterpret_cast<DestroyPluginFunc>(
        getSymbol(handle.libraryHandle, "destroyPlugin"));
    
    if (destroyFunc) {
        destroyFunc(handle.instance.get());
    }
    
    unloadLibrary(handle.libraryHandle);
    plugins.erase(it);
    
    std::cout << "Unloaded plugin: " << name << std::endl;
}

void PluginManager::reloadPlugin(const std::string& name) {
    std::lock_guard<std::mutex> lock(pluginsMutex);
    
    auto it = plugins.find(name);
    if (it == plugins.end()) {
        throw std::runtime_error("Plugin not loaded: " + name);
    }
    
    std::string path_str = it->second.path.string();
    unloadPlugin(name);
    loadPlugin(path_str);
}

void PluginManager::scanForPlugins(const std::string& directory) {
    namespace fs = std::filesystem;
    
#ifdef _WIN32
    std::wstring wdir = utf8_to_wstring(directory);
    if (!fs::exists(wdir)) {
#else
    if (!fs::exists(directory)) {
#endif
        std::cerr << "Plugin directory not found: " << directory << std::endl;
        return;
    }
    
    std::string ext = getPlatformExtension();
    
#ifdef _WIN32
    for (const auto& entry : fs::directory_iterator(wdir)) {
        // 将路径转换为 UTF-8 字符串
        std::string path_str = wstring_to_utf8(entry.path().wstring());
        
        if (!entry.is_regular_file()) continue;
        
        // 比较扩展名（使用宽字符）
        std::wstring wext(ext.begin(), ext.end());
        if (entry.path().extension().wstring() != wext) continue;
        
        try {
            loadPlugin(path_str);  // 传递字符串
        } catch (const std::exception& e) {
            std::cerr << "Failed to load plugin " << path_str << ": " << e.what() << std::endl;
        }
    }
#else
    for (const auto& entry : fs::directory_iterator(directory)) {
        // 获取路径的字符串表示
        std::string path_str = entry.path().string();
        
        if (!entry.is_regular_file()) continue;
        
        // 比较扩展名
        if (entry.path().extension().string() != ext) continue;
        
        try {
            // 确保传递字符串而不是路径对象
            loadPlugin(path_str);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load plugin " << path_str << ": " << e.what() << std::endl;
        }
    }
#endif
}

void PluginManager::monitoringThreadFunc() {
    while (monitoring) {
        std::this_thread::sleep_for(2s);
        
        std::vector<std::string> pluginsToReload;
        {
            std::lock_guard<std::mutex> lock(pluginsMutex);
            namespace fs = std::filesystem;
            
            for (auto& [name, handle] : plugins) {
                try {
#ifdef _WIN32
                    std::wstring wpath = utf8_to_wstring(handle.path.string());
                    auto currentTime = fs::last_write_time(wpath);
#else
                    auto currentTime = fs::last_write_time(handle.path);
#endif
                    if (currentTime != handle.lastWriteTime) {
                        std::cout << "Detected change in plugin: " << name << std::endl;
                        pluginsToReload.push_back(name);
                        handle.lastWriteTime = currentTime;
                    }
                } catch (...) {
                    // 文件可能暂时不可用
                }
            }
        }
        
        for (const auto& name : pluginsToReload) {
            try {
                reloadPlugin(name);
            } catch (const std::exception& e) {
                std::cerr << "Failed to reload plugin: " << name << " - " << e.what() << std::endl;
            }
        }
    }
}

void PluginManager::startMonitoring() {
    if (monitoring) return;
    
    monitoring = true;
    monitorThread = std::thread(&PluginManager::monitoringThreadFunc, this);
}

void PluginManager::stopMonitoring() {
    if (!monitoring) return;
    
    monitoring = false;
    if (monitorThread.joinable()) {
        monitorThread.join();
    }
}

std::vector<std::string> PluginManager::getLoadedPlugins() const {
    std::vector<std::string> names;
    std::lock_guard<std::mutex> lock(pluginsMutex);
    
    for (const auto& [name, _] : plugins) {
        names.push_back(name);
    }
    
    return names;
}