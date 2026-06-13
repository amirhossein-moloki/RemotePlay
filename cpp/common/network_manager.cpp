#include "network_manager.hpp"
#include <iostream>

#ifdef _WIN32
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

namespace Network {

std::vector<InterfaceInfo> NetworkManager::EnumerateInterfaces() {
    std::vector<InterfaceInfo> interfaces;
#ifdef _WIN32
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses != NULL; pCurrAddresses = pCurrAddresses->Next) {
            InterfaceInfo info;
            std::wstring wname = pCurrAddresses->FriendlyName;
            info.name = std::string(wname.begin(), wname.end());
            info.isActive = (pCurrAddresses->OperStatus == IfOperStatusUp);

            for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next) {
                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    char buf[INET_ADDRSTRLEN];
                    getnameinfo(pUnicast->Address.lpSockaddr, pUnicast->Address.iSockaddrLength, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
                    info.ip = buf;
                    interfaces.push_back(info);
                }
            }
        }
    }
    free(pAddresses);
#else
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return interfaces;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;

        InterfaceInfo info;
        info.name = ifa->ifa_name;
        info.isActive = (ifa->ifa_flags & IFF_UP);
        char host[NI_MAXHOST];
        getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        info.ip = host;
        interfaces.push_back(info);
    }
    freeifaddrs(ifaddr);
#endif
    return interfaces;
}

NetworkManager::NetworkManager() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    m_socket = INVALID_SOCKET;
#else
    m_socket = -1;
#endif
}

NetworkManager::~NetworkManager() {
#ifdef _WIN32
    if (m_socket != INVALID_SOCKET) closesocket(m_socket);
    WSACleanup();
#else
    if (m_socket != -1) close(m_socket);
#endif
}

bool NetworkManager::Bind(const std::string& ip, uint16_t port) {
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (m_socket == INVALID_SOCKET) return false;
#else
    if (m_socket < 0) return false;
#endif

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return false;
    }
    return true;
}

int NetworkManager::SendTo(const void* data, size_t size, const std::string& targetIp, uint16_t targetPort) {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(targetPort);
    inet_pton(AF_INET, targetIp.c_str(), &addr.sin_addr);
    return sendto(m_socket, (const char*)data, (int)size, 0, (struct sockaddr*)&addr, sizeof(addr));
}

int NetworkManager::SendBatch(const BatchItem* items, size_t count) {
    if (count == 0) return 0;

#ifdef _WIN32
    // Windows: Use WSASendTo with multiple buffers if possible,
    // but WSASendTo doesn't support multiple target addresses in one call easily like sendmmsg.
    // However, for single client (typical LAN), we can still benefit from reducing syscalls if we had multiple buffers for SAME target.
    // Given the current BatchItem has different targets, we fall back to loop for now on Windows unless we optimize for same-target.
    int totalSent = 0;
    for (size_t i = 0; i < count; ++i) {
        int ret = SendTo(items[i].data, items[i].size, items[i].targetIp, items[i].targetPort);
        if (ret > 0) totalSent += ret;
    }
    return totalSent;
#else
    // Linux: Use sendmmsg
    struct mmsghdr msgs[64];
    struct iovec iovs[64];
    struct sockaddr_in addrs[64];

    size_t batchSize = std::min(count, (size_t)64);
    for (size_t i = 0; i < batchSize; ++i) {
        addrs[i].sin_family = AF_INET;
        addrs[i].sin_port = htons(items[i].targetPort);
        inet_pton(AF_INET, items[i].targetIp.c_str(), &addrs[i].sin_addr);

        iovs[i].iov_base = (void*)items[i].data;
        iovs[i].iov_len = items[i].size;

        msgs[i].msg_hdr.msg_name = &addrs[i];
        msgs[i].msg_hdr.msg_namelen = sizeof(addrs[i]);
        msgs[i].msg_hdr.msg_iov = &iovs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
        msgs[i].msg_hdr.msg_control = NULL;
        msgs[i].msg_hdr.msg_controllen = 0;
        msgs[i].msg_hdr.msg_flags = 0;
    }

    return sendmmsg(m_socket, msgs, batchSize, 0);
#endif
}

int NetworkManager::ReceiveFrom(void* buffer, size_t maxSize, std::string& senderIp, uint16_t& senderPort) {
    sockaddr_in addr;
#ifdef _WIN32
    int addrLen = sizeof(addr);
#else
    socklen_t addrLen = sizeof(addr);
#endif
    int bytesReceived = recvfrom(m_socket, (char*)buffer, (int)maxSize, 0, (struct sockaddr*)&addr, &addrLen);

    if (bytesReceived > 0) {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
        senderIp = buf;
        senderPort = ntohs(addr.sin_port);
    }
    return bytesReceived;
}

} // namespace Network
