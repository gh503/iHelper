#include "CorePlatform/Network.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <netinet/ip_icmp.h>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <fcntl.h>
#include <cerrno>
#include <netinet/tcp.h>

namespace CorePlatform {

// =============== IPAddress 实现 ===============
IPAddress::IPAddress() : m_type(Type::Invalid), m_ipv4(0) {}

IPAddress::IPAddress(const std::string& address) {
    in_addr ipv4;
    if (inet_pton(AF_INET, address.c_str(), &ipv4) == 1) {
        m_type = Type::IPv4;
        m_ipv4 = ntohl(ipv4.s_addr);
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
    : m_type(Type::IPv4), m_ipv4(ipv4) {}

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
        addr.s_addr = htonl(m_ipv4);
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
    return m_ipv4;
}

std::array<uint8_t, 16> IPAddress::ToIPv6() const {
    if (m_type != Type::IPv6) {
        throw std::logic_error("Not an IPv6 address");
    }
    return m_ipv6;
}

IPAddress IPAddress::Localhost(Type type) {
    if (type == Type::IPv4) {
        return IPAddress(ntohl(INADDR_LOOPBACK));
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
    int handle = -1;
    CPSocket::Type sockType = CPSocket::Type::TCP;
    bool connected = false;
    
    ~Impl() {
        Disconnect();
    }
    
    void Disconnect() {
        if (handle >= 0) {
            close(handle);
            handle = -1;
        }
        connected = false;
    }
};

CPSocket::CPSocket() : pImpl(std::make_unique<Impl>()) {}

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
    if (pImpl->handle < 0) return false;

    // 设置非阻塞以支持超时
    int flags = fcntl(pImpl->handle, F_GETFL, 0);
    fcntl(pImpl->handle, F_SETFL, flags | O_NONBLOCK);

    bool connectSuccess = false;
    if (address.GetType() == IPAddress::Type::IPv4) {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(address.ToIPv4());
        
        int result = connect(pImpl->handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (result == 0) {
            connectSuccess = true;
        } else if (errno == EINPROGRESS) {
            // 使用select等待连接完成
            fd_set set;
            FD_ZERO(&set);
            FD_SET(pImpl->handle, &set);
            
            timeval timeout = {5, 0}; // 5秒超时
            if (select(pImpl->handle + 1, nullptr, &set, nullptr, &timeout) > 0) {
                int error = 0;
                socklen_t len = sizeof(error);
                getsockopt(pImpl->handle, SOL_SOCKET, SO_ERROR, &error, &len);
                connectSuccess = (error == 0);
            }
        }
    } 
    else {
        sockaddr_in6 addr = {};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        auto ipv6 = address.ToIPv6();
        memcpy(&addr.sin6_addr, ipv6.data(), 16);
        
        int result = connect(pImpl->handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (result == 0) {
            connectSuccess = true;
        } else if (errno == EINPROGRESS) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(pImpl->handle, &set);
            
            timeval timeout = {5, 0}; // 5秒超时
            if (select(pImpl->handle + 1, nullptr, &set, nullptr, &timeout) > 0) {
                int error = 0;
                socklen_t len = sizeof(error);
                getsockopt(pImpl->handle, SOL_SOCKET, SO_ERROR, &error, &len);
                connectSuccess = (error == 0);
            }
        }
    }

    // 恢复阻塞模式
    fcntl(pImpl->handle, F_SETFL, flags & ~O_NONBLOCK);

    if (!connectSuccess) {
        pImpl->Disconnect();
        return false;
    }

    pImpl->connected = true;
    return true;
}

void CPSocket::Disconnect() {
    pImpl->Disconnect();
}

int CPSocket::Send(const void* data, size_t size) {
    if (!pImpl->connected) return -1;
    return send(pImpl->handle, data, size, 0);
}

int CPSocket::Receive(void* buffer, size_t size) {
    if (!pImpl->connected) return -1;
    return recv(pImpl->handle, buffer, size, 0);
}

bool CPSocket::IsConnected() const {
    return pImpl->connected;
}

bool CPSocket::SetTimeout(int milliseconds) {
    if (pImpl->handle < 0) return false;

    struct timeval tv = {
        .tv_sec = milliseconds / 1000,
        .tv_usec = (milliseconds % 1000) * 1000
    };
    if (setsockopt(pImpl->handle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
        return false;
    }
    return setsockopt(pImpl->handle, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == 0;
}

std::unique_ptr<CPSocket> CPSocket::Create() {
    return std::make_unique<CPSocket>();
}

// =============== Network 实现 ===============
std::vector<NetworkInterface> Network::GetInterfaces() {
    std::vector<NetworkInterface> interfaces;
    struct ifaddrs* ifap;
    if (getifaddrs(&ifap) != 0) return interfaces;

    for (struct ifaddrs* ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || !ifa->ifa_name) continue;

        NetworkInterface intf;
        intf.name = ifa->ifa_name;
        intf.description = ifa->ifa_name;
        intf.isUp = (ifa->ifa_flags & IFF_UP);
        intf.isLoopback = (ifa->ifa_flags & IFF_LOOPBACK);
        
        // 获取MTU
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            struct ifreq ifr = {};
            strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ);
            if (ioctl(sock, SIOCGIFMTU, &ifr) >= 0) {
                intf.mtu = ifr.ifr_mtu;
            }
            close(sock);
        }

        // 处理IPv4地址
        if (ifa->ifa_addr->sa_family == AF_INET) {
            sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            intf.address = IPAddress(ntohl(sin->sin_addr.s_addr));
            
            if (ifa->ifa_netmask) {
                sockaddr_in* mask_sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_netmask);
                intf.netmask = IPAddress(ntohl(mask_sin->sin_addr.s_addr));
            }
            
            if (ifa->ifa_flags & IFF_BROADCAST && ifa->ifa_broadaddr) {
                sockaddr_in* bcast = reinterpret_cast<sockaddr_in*>(ifa->ifa_broadaddr);
                intf.broadcast = IPAddress(ntohl(bcast->sin_addr.s_addr));
            }
        }
        // 处理IPv6地址
        else if (ifa->ifa_addr->sa_family == AF_INET6) {
            sockaddr_in6* sin6 = reinterpret_cast<sockaddr_in6*>(ifa->ifa_addr);
            std::array<uint8_t, 16> bytes;
            memcpy(bytes.data(), &sin6->sin6_addr, 16);
            intf.address = IPAddress(bytes);
            
            if (ifa->ifa_netmask) {
                sockaddr_in6* mask_sin6 = reinterpret_cast<sockaddr_in6*>(ifa->ifa_netmask);
                std::array<uint8_t, 16> mask_bytes;
                memcpy(mask_bytes.data(), &mask_sin6->sin6_addr, 16);
                intf.netmask = IPAddress(mask_bytes);
            }
        } else {
            continue;
        }

        interfaces.push_back(std::move(intf));
    }
    
    freeifaddrs(ifap);
    return interfaces;
}

std::string Network::GetHostName() {
    char name[256] = {0};
    if (gethostname(name, sizeof(name)) == 0) {
        return name;
    }
    return "localhost";
}

std::vector<IPAddress> Network::ResolveHostName(const std::string& hostname) {
    std::vector<IPAddress> addresses;
    
    struct addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) != 0) {
        return addresses;
    }

    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(p->ai_addr);
            addresses.emplace_back(ntohl(sin->sin_addr.s_addr));
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
        // 记录错误日志: Logger::Error("ICMP 句柄创建失败，错误代码: {}", error);
        return false;
    }

    auto cleanup = [&]() { IcmpCloseHandle(hIcmp); };

    const char sendData[32] = "PingData";
    constexpr DWORD REPLY_BUFFER_SIZE = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData);
    BYTE replyBuffer[REPLY_BUFFER_SIZE];
    
    if (address.GetType() == IPAddress::Type::IPv4) {
        // IPv4 部分保持不变
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
        // 准备目标地址
        sockaddr_in6 target;
        memset(&target, 0, sizeof(target));
        target.sin6_family = AF_INET6;
        auto ipv6 = address.ToIPv6();
        memcpy(&target.sin6_addr, ipv6.data(), sizeof(target.sin6_addr));
        
        // 准备源地址（对于本地回环特别处理）
        sockaddr_in6 source;
        memset(&source, 0, sizeof(source));
        source.sin6_family = AF_INET6;
        
        // 检查是否是本地回环地址
        const bool isLocalhost = (address == IPAddress::Localhost(IPAddress::Type::IPv6));
        if (isLocalhost) {
            // 设置源地址为 ::1
            source.sin6_addr.u.Byte[15] = 1;
        }
        
        // 获取本地适配器地址作为源地址（非本地回环时）
        if (!isLocalhost) {
            PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
            ULONG outBufLen = 0;
            DWORD dwRetVal = 0;
            
            // 获取适配器信息
            if (GetAdaptersAddresses(AF_INET6, 0, nullptr, nullptr, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
                pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
                if (pAddresses) {
                    dwRetVal = GetAdaptersAddresses(AF_INET6, 0, nullptr, pAddresses, &outBufLen);
                    if (dwRetVal == ERROR_SUCCESS) {
                        // 使用第一个 IPv6 适配器的地址作为源地址
                        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
                        while (pCurrAddresses) {
                            if (pCurrAddresses->OperStatus == IfOperStatusUp) {
                                PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                                if (pUnicast) {
                                    sockaddr_in6* pAddr = reinterpret_cast<sockaddr_in6*>(pUnicast->Address.lpSockaddr);
                                    if (pAddr->sin6_family == AF_INET6) {
                                        memcpy(&source.sin6_addr, &pAddr->sin6_addr, sizeof(source.sin6_addr));
                                        break;
                                    }
                                }
                            }
                            pCurrAddresses = pCurrAddresses->Next;
                        }
                    }
                    free(pAddresses);
                }
            }
        }
        
        // 调用 IPv6 Ping
        DWORD reply = Icmp6SendEcho2(
            hIcmp, 
            nullptr,    // Event
            nullptr,    // ApcRoutine
            nullptr,    // ApcContext
            isLocalhost ? (sockaddr_in6*)&source : nullptr, // 源地址
            (sockaddr_in6*)&target, // 目标地址
            const_cast<char*>(sendData), 
            static_cast<WORD>(sizeof(sendData)), 
            nullptr,    // RequestOptions
            replyBuffer,
            REPLY_BUFFER_SIZE,
            static_cast<DWORD>(timeoutMs)
        );
        
        if (reply == 0) {
            DWORD error = GetLastError();
            // 记录错误日志: Logger::Error("ICMPv6 Ping 失败，错误代码: {}", error);
            
            // 常见错误处理
            if (error == ERROR_ACCESS_DENIED) {
                // 记录日志: Logger::Error("需要管理员权限执行 Ping 操作");
            }
        }
        
        cleanup();
        return reply > 0;
    }

    cleanup();
    return false;
}

} // namespace CorePlatform