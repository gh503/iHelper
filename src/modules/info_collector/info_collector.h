#ifndef INFO_COLLECTOR_H
#define INFO_COLLECTOR_H

#include "ihelper/interfaces/imodule.h"
#include "ihelper/interfaces/iplatform_adapter.h"

class InfoCollector : public IModule {
public:
    std::string name() const override;
    std::string version() const override;
    
    void initialize(IMessageBus* messageBus) override;
    void start() override;
    void stop() override;
    
private:
    void collect_system_info();
    
    IMessageBus* messageBus_ = nullptr;
    std::unique_ptr<IPlatformAdapter> platformAdapter_;
    bool active_ = false;
    std::thread collector_thread_;
};

#endif // INFO_COLLECTOR_H