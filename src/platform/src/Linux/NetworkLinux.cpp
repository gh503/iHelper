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

    int sock = (address.GetType() == IPAddress::Type::IPv4) ? 
        socket(AF_INET, SOCK_RAW, IPPROTO_ICMP) : 
        socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    
    if (sock < 0) return false;

    // 设置超时
    struct timeval tv = {
        .tv_sec = timeoutMs / 1000,
        .tv_usec = (timeoutMs % 1000) * 1000
    };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    bool success = false;
    if (address.GetType() == IPAddress::Type::IPv4) {
        sockaddr_in dest = {};
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = htonl(address.ToIPv4());
        
        struct icmphdr packet;
        memset(&packet, 0, sizeof(packet));
        packet.type = ICMP_ECHO;
        packet.un.echo.id = getpid() & 0xFFFF;
        packet.un.echo.sequence = 1;
        packet.checksum = 0;
        
        // 计算校验和
        uint16_t* data = reinterpret_cast<uint16_t*>(&packet);
        size_t len = sizeof(packet);
        uint32_t sum = 0;
        while (len > 1) {
            sum += *data++;
            len -= 2;
        }
        if (len > 0) sum += *(uint8_t*)data;
        sum = (sum >> 16) + (sum & 0xFFFF);
        sum += (sum >> 16);
        packet.checksum = static_cast<uint16_t>(~sum);
        
        // 发送请求
        if (sendto(sock, &packet, sizeof(packet), 0, 
                  reinterpret_cast<sockaddr*>(&dest), sizeof(dest)) <= 0) {
            close(sock);
            return false;
        }
        
        // 接收响应
        char recvBuf[128];
        struct sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        if (recvfrom(sock, recvBuf, sizeof(recvBuf), 0, 
                    reinterpret_cast<sockaddr*>(&from), &fromLen) > 0) {
            success = true;
        }
    } 
    else if (address.GetType() == IPAddress::Type::IPv6) {
        sockaddr_in6 dest = {};
        dest.sin6_family = AF_INET6;
        auto ipv6 = address.ToIPv6();
        memcpy(&dest.sin6_addr, ipv6.data(), 16);
        
        struct icmp6_hdr packet;
        memset(&packet, 0, sizeof(packet));
        packet.icmp6_type = ICMP6_ECHO_REQUEST;
        packet.icmp6_code = 0;
        packet.icmp6_id = getpid() & 0xFFFF;
        packet.icmp6_seq = 1;
        
        // 发送请求
        if (sendto(sock, &packet, sizeof(packet), 0, 
                  reinterpret_cast<sockaddr*>(&dest), sizeof(dest)) <= 0) {
            close(sock);
            return false;
        }
        
        // 接收响应
        char recvBuf[128];
        struct sockaddr_in6 from;
        socklen_t fromLen = sizeof(from);
        if (recvfrom(sock, recvBuf, sizeof(recvBuf), 0, 
                    reinterpret_cast<sockaddr*>(&from), &fromLen) > 0) {
            success = true;
        }
    }

    close(sock);
    return success;
}

} // namespace CorePlatform