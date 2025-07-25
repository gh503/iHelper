#include "CorePlatform/Network.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <memory>
#include <stdexcept>
#include "CorePlatform/Windows/WindowsUtils.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace CorePlatform {

// =============== IPAddress 实现 ===============
IPAddress::IPAddress() : m_type(Type::Invalid), m_ipv4(0) {}

IPAddress::IPAddress(const std::string& address) {
    in_addr ipv4;
    if (inet_pton(AF_INET, address.c_str(), &ipv4) == 1) {
        m_type = Type::IPv4;
        m_ipv4 = ipv4.s_addr;
        return;
    }

    in6_addr ipv6;
    if (inet_pton(AF_INET6, address.c_str(), &ipv6) == 1) {
        m_type = Type::IPv6;
        memcpy(m_ipv6.data(), &ipv6, 16);
        return;
    }

    m_type = Type::Invalid;
    m_ipv4 = 0;
}

IPAddress::IPAddress(uint32_t ipv4) 
    : m_type(Type::IPv4), m_ipv4(htonl(ipv4)) {}

IPAddress::IPAddress(const std::array<uint8_t, 16>& ipv6) 
    : m_type(Type::IPv6), m_ipv6(ipv6) {}

bool IPAddress::IsValid() const {
    return m_type != Type::Invalid;
}

IPAddress::Type IPAddress::GetType() const {
    return m_type;
}

std::string IPAddress::ToString() const {
    char buffer[INET6_ADDRSTRLEN] = {0};
    
    if (m_type == Type::IPv4) {
        in_addr addr;
        addr.s_addr = m_ipv4;
        inet_ntop(AF_INET, &addr, buffer, sizeof(buffer));
        return buffer;
    } 
    else if (m_type == Type::IPv6) {
        in6_addr addr;
        memcpy(&addr, m_ipv6.data(), 16);
        inet_ntop(AF_INET6, &addr, buffer, sizeof(buffer));
        return buffer;
    }
    
    return "Invalid";
}

uint32_t IPAddress::ToIPv4() const {
    if (m_type != Type::IPv4) {
        throw std::logic_error("Not an IPv4 address");
    }
    return ntohl(m_ipv4);
}

std::array<uint8_t, 16> IPAddress::ToIPv6() const {
    if (m_type != Type::IPv6) {
        throw std::logic_error("Not an IPv6 address");
    }
    return m_ipv6;
}

IPAddress IPAddress::Localhost(Type type) {
    if (type == Type::IPv4) {
        return IPAddress(INADDR_LOOPBACK);
    } else if (type == Type::IPv6) {
        std::array<uint8_t, 16> ipv6 = {0};
        ipv6[15] = 1;
        return IPAddress(ipv6);
    }
    return IPAddress();
}

bool IPAddress::operator==(const IPAddress& other) const {
    if (m_type != other.m_type) return false;
    if (m_type == Type::Invalid) return true;
    if (m_type == Type::IPv4) return m_ipv4 == other.m_ipv4;
    return memcmp(m_ipv6.data(), other.m_ipv6.data(), 16) == 0;
}

bool IPAddress::operator!=(const IPAddress& other) const {
    return !(*this == other);
}

// =============== CPSocket 实现 ===============
struct CPSocket::Impl {
    SOCKET handle = INVALID_SOCKET;
    CPSocket::Type sockType = CPSocket::Type::TCP;
    bool connected = false;
    
    ~Impl() {
        Disconnect();
    }
    
    void Disconnect() {
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
            handle = INVALID_SOCKET;
        }
        connected = false;
    }
};

CPSocket::CPSocket() : pImpl(std::make_unique<Impl>()) {
    // 初始化Winsock
    static bool wsaInitialized = false;
    if (!wsaInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            wsaInitialized = true;
        }
    }
}

CPSocket::~CPSocket() = default;

CPSocket::CPSocket(CPSocket&& other) noexcept 
    : pImpl(std::move(other.pImpl)) {}

CPSocket& CPSocket::operator=(CPSocket&& other) noexcept {
    if (this != &other) {
        pImpl = std::move(other.pImpl);
    }
    return *this;
}

bool CPSocket::Connect(const IPAddress& address, uint16_t port, Type type) {
    if (!address.IsValid()) { return false; }
    pImpl->Disconnect();
    pImpl->sockType = type;

    int af = (address.GetType() == IPAddress::Type::IPv4) ? AF_INET : AF_INET6;
    int socktype = (type == Type::TCP) ? SOCK_STREAM : SOCK_DGRAM;
    int protocol = (type == Type::TCP) ? IPPROTO_TCP : IPPROTO_UDP;

    pImpl->handle = socket(af, socktype, protocol);
    if (pImpl->handle == INVALID_SOCKET) return false;

    if (address.GetType() == IPAddress::Type::IPv4) {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(address.ToIPv4());
        
        if (::connect(pImpl->handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            pImpl->Disconnect();
            return false;
        }
    } 
    else {
        sockaddr_in6 addr = {};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        auto ipv6 = address.ToIPv6();
        memcpy(&addr.sin6_addr, ipv6.data(), 16);
        
        if (::connect(pImpl->handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            pImpl->Disconnect();
            return false;
        }
    }

    pImpl->connected = true;
    return true;
}

void CPSocket::Disconnect() {
    pImpl->Disconnect();
}

int CPSocket::Send(const void* data, size_t size) {
    if (!pImpl->connected) return -1;
    return send(pImpl->handle, static_cast<const char*>(data), static_cast<int>(size), 0);
}

int CPSocket::Receive(void* buffer, size_t size) {
    if (!pImpl->connected) return -1;
    return recv(pImpl->handle, static_cast<char*>(buffer), static_cast<int>(size), 0);
}

bool CPSocket::IsConnected() const {
    return pImpl->connected;
}

bool CPSocket::SetTimeout(int milliseconds) {
    if (pImpl->handle == INVALID_SOCKET) return false;

    DWORD timeout = milliseconds;
    if (setsockopt(pImpl->handle, SOL_SOCKET, SO_RCVTIMEO, 
                  reinterpret_cast<char*>(&timeout), sizeof(timeout)) != 0) {
        return false;
    }
    return setsockopt(pImpl->handle, SOL_SOCKET, SO_SNDTIMEO, 
                    reinterpret_cast<char*>(&timeout), sizeof(timeout)) == 0;
}

std::unique_ptr<CPSocket> CPSocket::Create() {
    return std::make_unique<CPSocket>();
}

// =============== Network 实现 ===============
std::vector<NetworkInterface> Network::GetInterfaces() {
    std::vector<NetworkInterface> interfaces;

    ULONG size = 0;
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &size);
    if (size == 0) return interfaces;

    std::vector<BYTE> buffer(size);
    PIP_ADAPTER_ADDRESSES adapterAddrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapterAddrs, &size) != ERROR_SUCCESS) {
        return interfaces;
    }

    for (PIP_ADAPTER_ADDRESSES adapter = adapterAddrs; adapter != nullptr; adapter = adapter->Next) {
        bool isUp = (adapter->OperStatus == IfOperStatusUp);
        bool isLoopback = (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK);

        for (PIP_ADAPTER_UNICAST_ADDRESS addr = adapter->FirstUnicastAddress; 
             addr != nullptr; 
             addr = addr->Next) {
            
            NetworkInterface intf;
            intf.name = WindowsUtils::WideToUTF8(adapter->FriendlyName);
            intf.description = WindowsUtils::WideToUTF8(adapter->Description);
            
            intf.mtu = adapter->Mtu;
            if (intf.mtu == (ULONG)-1) {
                intf.mtu = 0;
            }
            
            intf.isUp = isUp;
            intf.isLoopback = isLoopback;

            // 获取IP地址
            sockaddr* sa = addr->Address.lpSockaddr;
            if (sa->sa_family == AF_INET) {
                sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(sa);
                // 关键修复：将网络字节序转换为主机字节序
                uint32_t ipv4HostOrder = ntohl(sin->sin_addr.s_addr);
                intf.address = IPAddress(ipv4HostOrder);
                
                // 获取子网掩码
                ULONG prefix = addr->OnLinkPrefixLength;
                if (prefix <= 32) {
                    uint32_t mask = (prefix == 0) ? 0 : ~((1 << (32 - prefix)) - 1);
                    // 子网掩码也需要转换字节序
                    intf.netmask = IPAddress(htonl(mask));
                }
            } 
            else if (sa->sa_family == AF_INET6) {
                sockaddr_in6* sin6 = reinterpret_cast<sockaddr_in6*>(sa);
                std::array<uint8_t, 16> bytes;
                memcpy(bytes.data(), &sin6->sin6_addr, 16);
                intf.address = IPAddress(bytes);
                
                // IPv6子网掩码
                ULONG prefix = addr->OnLinkPrefixLength;
                std::array<uint8_t, 16> mask = {};
                for (ULONG i = 0; i < 16; i++) {
                    if (prefix >= 8) {
                        mask[i] = 0xFF;
                        prefix -= 8;
                    } else if (prefix > 0) {
                        mask[i] = static_cast<uint8_t>(0xFF << (8 - prefix));
                        prefix = 0;
                    }
                }
                intf.netmask = IPAddress(mask);
            } else {
                continue;
            }

            interfaces.push_back(std::move(intf));
        }
    }

    return interfaces;
}

std::string Network::GetHostName() {
    wchar_t name[256] = {0};
    DWORD size = sizeof(name) / sizeof(name[0]);
    if (GetComputerNameW(name, &size)) {
        return WindowsUtils::WideToUTF8(name);
    }
    return "localhost";
}

std::vector<IPAddress> Network::ResolveHostName(const std::string& hostname) {
    std::vector<IPAddress> addresses;
    
    // 确保 Winsock 已初始化
    static bool wsaInitialized = []() {
        WSADATA wsaData;
        return (WSAStartup(MAKEWORD(2, 2), &wsaData)) == 0;
    }();
    (void)wsaInitialized; // 显式忽略变量，消除警告
    
    struct addrinfo hints = {};
    struct addrinfo* res = nullptr;
    
    // 设置查询参数
    hints.ai_family = AF_UNSPEC;     // 返回 IPv4 和 IPv6 地址
    hints.ai_socktype = SOCK_STREAM; // 流式套接字类型
    hints.ai_protocol = IPPROTO_TCP; // TCP 协议
    hints.ai_flags = AI_CANONNAME;   // 返回规范名称
    
    // 特殊处理 "localhost"
    std::string target = hostname;
    if (hostname == "localhost" || hostname == "localhost.") {
        // 尝试解析 IPv4 和 IPv6 的 localhost
        addresses.emplace_back(IPAddress::Localhost(IPAddress::Type::IPv4));
        addresses.emplace_back(IPAddress::Localhost(IPAddress::Type::IPv6));
        return addresses;
    }
    
    // 执行 DNS 查询
    int result = getaddrinfo(target.c_str(), nullptr, &hints, &res);
    if (result != 0) {
        // 尝试不带标志的查询
        hints.ai_flags = 0;
        result = getaddrinfo(target.c_str(), nullptr, &hints, &res);
        if (result != 0) {
            return addresses;
        }
    }
    
    // 遍历所有结果
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(p->ai_addr);
            addresses.emplace_back(sin->sin_addr.s_addr);
        } 
        else if (p->ai_family == AF_INET6) {
            sockaddr_in6* sin6 = reinterpret_cast<sockaddr_in6*>(p->ai_addr);
            std::array<uint8_t, 16> bytes;
            memcpy(bytes.data(), &sin6->sin6_addr, 16);
            addresses.emplace_back(bytes);
        }
    }
    
    freeaddrinfo(res);
    return addresses;
}

bool Network::Ping(const IPAddress& address, int timeoutMs) {
    if (!address.IsValid()) return false;

    // 创建正确的 ICMP 句柄
    HANDLE hIcmp = (address.GetType() == IPAddress::Type::IPv4) 
        ? IcmpCreateFile() 
        : Icmp6CreateFile();
    
    if (hIcmp == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::cerr << "ICMP handle creation failed, error: " << error << std::endl;
        return false;
    }

    auto cleanup = [&]() { IcmpCloseHandle(hIcmp); };

    const char sendData[32] = "PingData";
    
    if (address.GetType() == IPAddress::Type::IPv4) {
        // IPv4 部分保持不变
        constexpr DWORD REPLY_BUFFER_SIZE = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData);
        BYTE replyBuffer[REPLY_BUFFER_SIZE];
        
        in_addr target;
        target.s_addr = htonl(address.ToIPv4());
        
        DWORD reply = IcmpSendEcho(
            hIcmp, 
            target.s_addr, 
            const_cast<char*>(sendData), 
            static_cast<WORD>(sizeof(sendData)),
            nullptr, 
            replyBuffer, 
            REPLY_BUFFER_SIZE, 
            timeoutMs
        );
        
        cleanup();
        return reply > 0;
    } 
    else if (address.GetType() == IPAddress::Type::IPv6) {
        constexpr DWORD REPLY_BUFFER_SIZE = sizeof(ICMPV6_ECHO_REPLY) + sizeof(sendData) + 8;
        std::vector<BYTE> replyBuffer(REPLY_BUFFER_SIZE);
        
        SOCKADDR_IN6 targetAddr = {};
        targetAddr.sin6_family = AF_INET6;
        
        auto ipv6 = address.ToIPv6();
        memcpy(&targetAddr.sin6_addr, ipv6.data(), sizeof(targetAddr.sin6_addr));
        
        // 2. 正确处理作用域 ID
        const bool isLocalhost = (memcmp(ipv6.data(), 
            "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1", 16) == 0);
        
        // 对于 localhost 使用作用域 ID 0
        targetAddr.sin6_scope_id = isLocalhost ? 0 : 1;
        
        // 3. 创建有效的源地址结构
        SOCKADDR_IN6 sourceAddr = {};
        sourceAddr.sin6_family = AF_INET6;
        sourceAddr.sin6_addr = in6addr_any; // 使用任意地址
        
        IP_OPTION_INFORMATION requestOptions = {};
        requestOptions.Ttl = 128;
        
        // 4. 使用有效的源地址
        DWORD reply = Icmp6SendEcho2(
            hIcmp,
            nullptr,    // Event
            nullptr,    // ApcRoutine
            nullptr,    // ApcContext
            &sourceAddr, // 使用有效的源地址
            &targetAddr, // 目标地址
            const_cast<char*>(sendData),
            static_cast<WORD>(sizeof(sendData)),
            &requestOptions,
            replyBuffer.data(),
            REPLY_BUFFER_SIZE,
            static_cast<DWORD>(timeoutMs)
        );
        
        if (reply == 0) {
            DWORD error = GetLastError();
            std::cerr << "Icmp6SendEcho2 failed with error: " << error << std::endl;
            
            if (error == IP_REQ_TIMED_OUT) {
                std::cerr << "Request timed out" << std::endl;
            }
        }
        
        cleanup();
        return reply > 0;
    }

    cleanup();
    return false;
}

} // namespace CorePlatform