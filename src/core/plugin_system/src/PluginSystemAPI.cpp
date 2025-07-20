#include "PluginSystemAPI.h"
#include "PluginManager.h"
#include <iostream>
#include <sstream>
#include <memory>
#include <stdexcept>

class PluginSystem::Impl {
public:
    PluginManager manager;
    
    void printHelp() {
        std::cout << "\nPlugin System Commands:\n"
                  << "  load <path>    : Load a plugin from given path\n"
                  << "  unload <name>  : Unload a plugin by name\n"
                  << "  reload <name>  : Reload a plugin by name\n"
                  << "  list           : List all loaded plugins\n"
                  << "  scan [dir]     : Scan directory for plugins (default: plugins)\n"
                  << "  monitor        : Start plugin hot-reload monitoring\n"
                  << "  stop-monitor   : Stop plugin monitoring\n"
                  << "  help           : Show this help\n"
                  << "  exit           : Exit the command line\n";
    }
};

PluginSystem::PluginSystem() : pImpl(std::make_unique<Impl>()) {}

PluginSystem::~PluginSystem() = default;

PluginSystem& PluginSystem::getInstance() {
    static PluginSystem instance;
    return instance;
}

void PluginSystem::loadPlugin(const std::string& path) {
    pImpl->manager.loadPlugin(path);
}

void PluginSystem::unloadPlugin(const std::string& name) {
    pImpl->manager.unloadPlugin(name);
}

void PluginSystem::reloadPlugin(const std::string& name) {
    pImpl->manager.reloadPlugin(name);
}

void PluginSystem::scanDirectory(const std::string& directory) {
    pImpl->manager.scanForPlugins(directory);
}

void PluginSystem::startMonitoring() {
    pImpl->manager.startMonitoring();
}

void PluginSystem::stopMonitoring() {
    pImpl->manager.stopMonitoring();
}

std::vector<std::string> PluginSystem::getLoadedPlugins() const {
    return pImpl->manager.getLoadedPlugins();
}

std::string PluginSystem::getPlatformExtension() const {
    return pImpl->manager.getPlatformExtension();
}

void PluginSystem::executeCommand(const std::string& command) {
    if (command.empty()) return;
    
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    try {
        if (cmd == "load") {
            std::string path;
            if (iss >> path) {
                loadPlugin(path);
            } else {
                std::cerr << "Usage: load <path>" << std::endl;
            }
        } else if (cmd == "unload") {
            std::string name;
            if (iss >> name) {
                unloadPlugin(name);
            } else {
                std::cerr << "Usage: unload <name>" << std::endl;
            }
        } else if (cmd == "reload") {
            std::string name;
            if (iss >> name) {
                reloadPlugin(name);
            } else {
                std::cerr << "Usage: reload <name>" << std::endl;
            }
        } else if (cmd == "list") {
            auto plugins = getLoadedPlugins();
            if (plugins.empty()) {
                std::cout << "No plugins loaded." << std::endl;
            } else {
                std::cout << "Loaded plugins (" << plugins.size() << "):" << std::endl;
                for (const auto& name : plugins) {
                    std::cout << "  " << name << std::endl;
                }
            }
        } else if (cmd == "scan") {
            std::string dir = "plugins";
            iss >> dir;
            scanDirectory(dir);
            std::cout << "Scanned directory: " << dir << std::endl;
        } else if (cmd == "monitor") {
            startMonitoring();
            std::cout << "Plugin monitoring started" << std::endl;
        } else if (cmd == "stop-monitor") {
            stopMonitoring();
            std::cout << "Plugin monitoring stopped" << std::endl;
        } else if (cmd == "help") {
            pImpl->printHelp();
        } else {
            std::cout << "Unknown command. Type 'help' for assistance." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void PluginSystem::runCommandLineInterface() {
    std::cout << "=== Plugin System Command Line ===" << std::endl;
    std::cout << "Supported extensions: " << getPlatformExtension() << std::endl;
    pImpl->printHelp();
    
    std::string command;
    bool running = true;
    
    while (running) {
        std::cout << "plugin> ";
        std::getline(std::cin, command);
        
        if (command == "exit") {
            running = false;
            continue;
        }
        
        executeCommand(command);
    }
    
    std::cout << "Exiting plugin command line" << std::endl;
}