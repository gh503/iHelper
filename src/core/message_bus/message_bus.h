#ifndef MESSAGE_BUS_H
#define MESSAGE_BUS_H

#include <map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include "ihelper/interfaces/imessage_bus.h"

class MessageBus : public IMessageBus {
public:
    void publish(const std::string& topic, const nlohmann::json& message) override;
    void subscribe(const std::string& topic, MessageHandler handler) override;
    void unsubscribe(const std::string& topic) override;
    
private:
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::vector<MessageHandler>> handlers_;
};

#endif // MESSAGE_BUS_H