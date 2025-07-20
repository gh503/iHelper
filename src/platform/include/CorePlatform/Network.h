#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <functional>

namespace CorePlatform {

class IPAddress {
public:
    enum class Type { IPv4, IPv6, Invalid };
    
    IPAddress();
    explicit IPAddress(const std::string& address);
    explicit IPAddress(uint32_t ipv4);
    explicit IPAddress(const std::array<uint8_t, 16>& ipv6);
    
    bool IsValid() const;
    Type GetType() const;
    
    std::string ToString() const;
    uint32_t ToIPv4() const;
    std::array<uint8_t, 16> ToIPv6() const;
    
    static IPAddress Localhost(Type type = Type::IPv4);
    
    bool operator==(const IPAddress& other) const;
    bool operator!=(const IPAddress& other) const;
    
private:
    Type m_type;
    union {
        uint32_t m_ipv4;
        std::array<uint8_t, 16> m_ipv6;
    };
};

struct NetworkInterface {
    std::string name;
    std::string description;
    IPAddress address;
    IPAddress netmask;
    IPAddress broadcast;
    uint32_t mtu;
    bool isUp;
    bool isLoopback;
};

class CPSocket {
public:
    enum class Type { TCP, UDP };
    
    CPSocket();
    ~CPSocket();
    
    // 禁止复制
    CPSocket(const CPSocket&) = delete;
    CPSocket& operator=(const CPSocket&) = delete;
    
    // 移动语义
    CPSocket(CPSocket&& other) noexcept;
    CPSocket& operator=(CPSocket&& other) noexcept;
    
    bool Connect(const IPAddress& address, uint16_t port, Type type = Type::TCP);
    void Disconnect();
    int Send(const void* data, size_t size);
    int Receive(void* buffer, size_t size);
    bool IsConnected() const;
    bool SetTimeout(int milliseconds);
    
    static std::unique_ptr<CPSocket> Create();

private:
    // 平台特定的私有实现
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

namespace Network {
    std::vector<NetworkInterface> GetInterfaces();
    std::string GetHostName();
    std::vector<IPAddress> ResolveHostName(const std::string& hostname);
    bool Ping(const IPAddress& address, int timeoutMs = 1000);
};

} // namespace CorePlatform