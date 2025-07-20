#include "PluginInterface.h"
#include <iostream>

class ExamplePlugin : public IPlugin {
public:
    void initialize()  {
        std::cout << "ExamplePlugin initialized!" << std::endl;
    }
    
    void execute()  {
        std::cout << "ExamplePlugin executing..." << std::endl;
    }
    
    void shutdown()  {
        std::cout << "ExamplePlugin shutting down..." << std::endl;
    }
    
    std::string getName() const  {
        return "ExamplePlugin";
    }
    
    std::string getVersion() const  {
        return "0.0.1";
    }
};

// Windows 需要显式导出函数
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C" {
    EXPORT IPlugin* createPlugin() {
        return new ExamplePlugin();
    }
    
    EXPORT void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}