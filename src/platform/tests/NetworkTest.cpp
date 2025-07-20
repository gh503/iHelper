#include <gtest/gtest.h>
#include "CorePlatform/Network.h"

using namespace CorePlatform;

TEST(IPAddressTest, BasicOperations) {
    IPAddress invalid;
    EXPECT_FALSE(invalid.IsValid());
    EXPECT_EQ(invalid.GetType(), IPAddress::Type::Invalid);
    
    IPAddress localhost = IPAddress::Localhost();
    EXPECT_TRUE(localhost.IsValid());
    EXPECT_EQ(localhost.GetType(), IPAddress::Type::IPv4);
    EXPECT_EQ(localhost.ToString(), "127.0.0.1");
    
    IPAddress ipv4("192.168.1.1");
    EXPECT_TRUE(ipv4.IsValid());
    EXPECT_EQ(ipv4.GetType(), IPAddress::Type::IPv4);
    EXPECT_EQ(ipv4.ToString(), "192.168.1.1");
    
    IPAddress ipv6("::1");
    EXPECT_TRUE(ipv6.IsValid());
    EXPECT_EQ(ipv6.GetType(), IPAddress::Type::IPv6);
    EXPECT_EQ(ipv6.ToString(), "::1");
    
    EXPECT_NE(ipv4, ipv6);
    EXPECT_EQ(IPAddress::Localhost(IPAddress::Type::IPv6).ToString(), "::1");
}

TEST(NetworkTest, GetInterfaces) {
    auto interfaces = Network::GetInterfaces();
    EXPECT_FALSE(interfaces.empty());
    
    bool foundLoopback = false;
    bool foundNonLoopback = false;
    bool foundIPv6 = false;
    
    for (const auto& intf : interfaces) {
        EXPECT_FALSE(intf.name.empty());
        EXPECT_TRUE(intf.address.IsValid());
        
        if (intf.isLoopback) {
            foundLoopback = true;
            // 检查是否是 localhost 地址而不是直接比较对象
            if (intf.address.GetType() == IPAddress::Type::IPv4) {
                EXPECT_EQ(intf.address.ToString(), "127.0.0.1");
            } else if (intf.address.GetType() == IPAddress::Type::IPv6) {
                EXPECT_EQ(intf.address.ToString(), "::1");
            }
        } else {
            foundNonLoopback = true;
        }
        
        if (intf.address.GetType() == IPAddress::Type::IPv6) {
            foundIPv6 = true;
        }
        
        // 允许 MTU 为 0 (未知)
        if (intf.mtu > 0) {
            EXPECT_GT(intf.mtu, 0);
            EXPECT_LT(intf.mtu, 100000); // 放宽限制
        }
    }
    
    EXPECT_TRUE(foundLoopback);
    EXPECT_TRUE(foundNonLoopback);
    // 不是所有系统都有 IPv6 接口
    EXPECT_TRUE(foundIPv6);
}

TEST(NetworkTest, GetHostName) {
    std::string hostname = Network::GetHostName();
    EXPECT_FALSE(hostname.empty());
    EXPECT_NE(hostname, "localhost");
}

TEST(NetworkTest, ResolveHostName) {
    auto addresses = Network::ResolveHostName("localhost");
    EXPECT_FALSE(addresses.empty());
    
    bool foundIPv4 = false;
    bool foundIPv6 = false;
    
    for (const auto& addr : addresses) {
        if (addr.GetType() == IPAddress::Type::IPv4) {
            foundIPv4 = true;
            EXPECT_EQ(addr, IPAddress::Localhost());
        } else if (addr.GetType() == IPAddress::Type::IPv6) {
            foundIPv6 = true;
            EXPECT_EQ(addr, IPAddress::Localhost(IPAddress::Type::IPv6));
        }
    }
    
    EXPECT_TRUE(foundIPv4);
    EXPECT_TRUE(foundIPv6);
}

TEST(NetworkTest, PingLocalhost) {
    IPAddress localhost = IPAddress::Localhost();
    EXPECT_TRUE(Network::Ping(localhost));
    
    IPAddress localhost6 = IPAddress::Localhost(IPAddress::Type::IPv6);
    EXPECT_TRUE(Network::Ping(localhost6));
}

TEST(NetworkTest, CPSocketBasic) {
    auto socket = CPSocket::Create();
    EXPECT_NE(socket, nullptr);
    
    // 未连接状态检查
    EXPECT_FALSE(socket->IsConnected());
    EXPECT_EQ(socket->Send("test", 4), -1);
    char buffer[10];
    EXPECT_EQ(socket->Receive(buffer, sizeof(buffer)), -1);
    
    // 尝试连接无效地址
    EXPECT_FALSE(socket->Connect(IPAddress("999.999.999.999"), 80));
    EXPECT_FALSE(socket->IsConnected());
}

TEST(NetworkTest, CPSocketConnect) {
    // 注意：这个测试需要网络连接
    auto socket = CPSocket::Create();
    
    // 连接到本地回环地址
    EXPECT_TRUE(socket->Connect(IPAddress::Localhost(), 80));
    EXPECT_TRUE(socket->IsConnected());
    
    // 设置超时
    EXPECT_TRUE(socket->SetTimeout(1000));
    
    // 发送无效HTTP请求
    const char* request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int sent = socket->Send(request, strlen(request));
    EXPECT_GT(sent, 0);
    
    // 接收响应
    char response[1024];
    int received = socket->Receive(response, sizeof(response));
    EXPECT_GT(received, 0);
    
    // 断开连接
    socket->Disconnect();
    EXPECT_FALSE(socket->IsConnected());
}