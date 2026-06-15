#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#endif

#include "parsec_lite_api.h"

namespace Network {

struct InterfaceInfo {
    std::string name;
    std::string ip;
    bool isActive;
};

class PARSEC_API NetworkManager {
public:
    static std::vector<InterfaceInfo> EnumerateInterfaces();

    NetworkManager();
    ~NetworkManager();

    bool Bind(const std::string& ip, uint16_t port);
    int SendTo(const void* data, size_t size, const std::string& targetIp, uint16_t targetPort);

    struct BatchItem {
        const void* data;
        size_t size;
        std::string targetIp;
        uint16_t targetPort;
    };
    int SendBatch(const BatchItem* items, size_t count);

    int ReceiveFrom(void* buffer, size_t maxSize, std::string& senderIp, uint16_t& senderPort);

private:
#ifdef _WIN32
    SOCKET m_socket;
#else
    int m_socket;
#endif
};

} // namespace Network
