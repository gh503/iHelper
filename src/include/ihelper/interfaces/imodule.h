#ifndef IHELPER_IMODULE_H
#define IHELPER_IMODULE_H

#include <string>
#include <memory>

class IMessageBus;

class IModule {
public:
    virtual ~IModule() = default;
    
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual void initialize(IMessageBus* messageBus) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

#endif // IHELPER_IMODULE_H