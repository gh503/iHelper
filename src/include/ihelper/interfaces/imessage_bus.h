#ifndef IHELPER_IMESSAGE_BUS_H
#define IHELPER_IMESSAGE_BUS_H

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

class IMessageBus {
public:
    using MessageHandler = std::function<void(const std::string&, const nlohmann::json&)>;
    
    virtual ~IMessageBus() = default;
    
    virtual void publish(const std::string& topic, const nlohmann::json& message) = 0;
    virtual void subscribe(const std::string& topic, MessageHandler handler) = 0;
    virtual void unsubscribe(const std::string& topic) = 0;
};

#endif // IHELPER_IMESSAGE_BUS_H