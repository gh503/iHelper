#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <vector>
#include <memory>
#include "ihelper/interfaces/imodule.h"

class ModuleManager {
public:
    ModuleManager();
    ~ModuleManager();
    
    void load_module(const std::string& path);
    void unload_module(IModule* module);
    void start_all();
    void stop_all();
    
    const std::vector<std::unique_ptr<IModule>>& modules() const { return modules_; }
    
private:
    struct ModuleHandle {
        void* handle = nullptr;
        IModule* module = nullptr;
    };
    
    std::vector<std::unique_ptr<IModule>> modules_;
    std::vector<ModuleHandle> handles_;
};

#endif // MODULE_MANAGER_H