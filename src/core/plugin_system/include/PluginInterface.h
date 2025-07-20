#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#include <string>

class IPlugin {
public:
    virtual ~IPlugin() {}
    virtual void initialize() = 0;
    virtual void execute() = 0;
    virtual void shutdown() = 0;
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
};

using CreatePluginFunc = IPlugin* (*)();
using DestroyPluginFunc = void (*)(IPlugin*);

#endif // PLUGIN_INTERFACE_H