#include "network_manager.hpp"
#include "logger.hpp"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

namespace Network {

std::vector<InterfaceInfo> NetworkManager::EnumerateInterfaces() {
    std::vector<InterfaceInfo> interfaces;
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return interfaces;
    }

    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
    if (pAddresses == NULL) return interfaces;

    ULONG ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
        if (pAddresses == NULL) return interfaces;
        ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen);
    }

    if (ret == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses != NULL; pCurrAddresses = pCurrAddresses->Next) {
            InterfaceInfo info;
            std::wstring wname = pCurrAddresses->FriendlyName;
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), NULL, 0, NULL, NULL);
            if (size_needed > 0) {
                info.name.resize(size_needed);
                WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), &info.name[0], size_needed, NULL, NULL);
            }
            info.isActive = (pCurrAddresses->OperStatus == IfOperStatusUp);

            for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next) {
                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    char buf[INET_ADDRSTRLEN] = { 0 };
                    if (getnameinfo(pUnicast->Address.lpSockaddr, pUnicast->Address.iSockaddrLength, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST) == 0) {
                        info.ip = buf;
                        interfaces.push_back(info);
                    }
                }
            }
        }
    }
    free(pAddresses);
    WSACleanup();
#else
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return interfaces;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;

        InterfaceInfo info;
        info.name = ifa->ifa_name;
        info.isActive = (ifa->ifa_flags & IFF_UP);
        char host[NI_MAXHOST] = { 0 };
        if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
            info.ip = host;
            interfaces.push_back(info);
        }
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
    if (m_socket == INVALID_SOCKET) {
        LOG_ERROR("Network", "Failed to create socket. Error: " + std::to_string(WSAGetLastError()));
        return false;
    }

    // Set receive timeout to allow threads to exit gracefully
    DWORD timeout = 500; // ms
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    if (m_socket < 0) return false;

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000; // 500ms
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("Network", "Invalid IP address: " + ip);
        return false;
    }

    if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        LOG_ERROR("Network", "Failed to bind to " + ip + ":" + std::to_string(port) + ". Error: " + std::to_string(err));
        if (err == 10049) {
            LOG_ERROR("Network", "The requested address " + ip + " is not valid in this context. Please ensure you selected a valid network interface.");
        }
#endif
        return false;
    }
    return true;
}

int NetworkManager::SendTo(const void* data, size_t size, const std::string& targetIp, uint16_t targetPort) {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(targetPort);
    if (inet_pton(AF_INET, targetIp.c_str(), &addr.sin_addr) <= 0) {
        return -1;
    }
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
    memset(msgs, 0, sizeof(msgs));

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
    memset(&addr, 0, sizeof(addr));
#ifdef _WIN32
    int addrLen = sizeof(addr);
#else
    socklen_t addrLen = sizeof(addr);
#endif
    int bytesReceived = recvfrom(m_socket, (char*)buffer, (int)maxSize, 0, (struct sockaddr*)&addr, &addrLen);

    if (bytesReceived < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAETIMEDOUT && err != WSAEWOULDBLOCK && err != WSAECONNRESET) {
            LOG_ERROR("Network", "recvfrom failed with error: " + std::to_string(err));
        }
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Network", "recvfrom failed with error: " + std::to_string(errno));
        }
#endif
    }

    if (bytesReceived > 0) {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
        senderIp = buf;
        senderPort = ntohs(addr.sin_port);
    }
    return bytesReceived;
}

bool NetworkManager::GetPublicEndpoint(const std::string& stunServer, uint16_t stunPort, std::string& publicIp, uint16_t& publicPort) {
#ifdef _WIN32
    if (m_socket == INVALID_SOCKET) return false;
#else
    if (m_socket == -1) return false;
#endif

    // Simplified STUN Binding Request (RFC 5389)
    #pragma pack(push, 1)
    struct STUNHeader {
        uint16_t type;
        uint16_t length;
        uint32_t cookie;
        uint8_t transactionId[12];
    };
    #pragma pack(pop)

    STUNHeader req;
    req.type = htons(0x0001); // Binding Request
    req.length = 0;
    req.cookie = htonl(0x2112A442);

    // Use proper transaction ID (simplified for this architectural foundational phase)
    std::memset(req.transactionId, 0xAC, 12);

    if (SendTo(&req, sizeof(req), stunServer, stunPort) <= 0) {
        LOG_ERROR("Network", "Failed to send STUN request to " + stunServer);
        return false;
    }

    uint8_t buf[512];
    std::string senderIp;
    uint16_t sPort;
    int len = ReceiveFrom(buf, sizeof(buf), senderIp, sPort);

    if (len >= (int)sizeof(STUNHeader)) {
        STUNHeader* resp = (STUNHeader*)buf;
        if (ntohs(resp->type) == 0x0101) { // Binding Success Response
            int offset = sizeof(STUNHeader);
            int payloadLen = ntohs(resp->length);

            // Bounds check for STUN payload
            if (offset + payloadLen > len) payloadLen = len - offset;

            while (offset + 4 <= len && offset + 4 + ntohs(*(uint16_t*)(buf + offset + 2)) <= len) {
                uint16_t attrType = ntohs(*(uint16_t*)(buf + offset));
                uint16_t attrLen = ntohs(*(uint16_t*)(buf + offset + 2));

                if (attrType == 0x0001 && attrLen >= 8) { // MAPPED-ADDRESS
                    uint16_t port = ntohs(*(uint16_t*)(buf + offset + 6));
                    struct in_addr addr;
                    memcpy(&addr, buf + offset + 8, 4);
                    char ipBuf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &addr, ipBuf, sizeof(ipBuf));
                    publicIp = ipBuf;
                    publicPort = port;
                    return true;
                } else if (attrType == 0x0020 && attrLen >= 8) { // XOR-MAPPED-ADDRESS
                    uint16_t xPort = ntohs(*(uint16_t*)(buf + offset + 6)) ^ (0x2112A442 >> 16);
                    uint32_t xIp = *(uint32_t*)(buf + offset + 8) ^ htonl(0x2112A442);
                    struct in_addr addr;
                    addr.s_addr = xIp;
                    char ipBuf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &addr, ipBuf, sizeof(ipBuf));
                    publicIp = ipBuf;
                    publicPort = xPort;
                    return true;
                }
                offset += 4 + attrLen;
                // STUN attributes are padded to 4-byte boundaries
                if (attrLen % 4 != 0) offset += (4 - (attrLen % 4));
            }
        }
    }

    LOG_WARN("Network", "STUN discovery failed for server " + stunServer);
    return false;
}

bool NetworkManager::GetPublicEndpointParallel(const std::vector<StunServer>& servers, std::string& publicIp, uint16_t& publicPort) {
    if (servers.empty()) return false;

    auto sendAll = [&]() {
        for (const auto& s : servers) {
            #pragma pack(push, 1)
            struct STUNHeader {
                uint16_t type;
                uint16_t length;
                uint32_t cookie;
                uint8_t transactionId[12];
            };
            #pragma pack(pop)

            STUNHeader req;
            req.type = htons(0x0001);
            req.length = 0;
            req.cookie = htonl(0x2112A442);
            for(int i=0; i<12; ++i) req.transactionId[i] = (uint8_t)(rand() % 256);

            SendTo(&req, sizeof(req), s.host, s.port);
        }
    };

    sendAll();

    // Wait for the first valid response with a timeout and retry
    auto start = std::chrono::steady_clock::now();
    auto lastSend = start;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < 3000) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSend).count() > 500) {
            sendAll(); // Retry every 500ms
            lastSend = now;
        }
        uint8_t buf[512];
        std::string senderIp;
        uint16_t sPort;
        int len = ReceiveFrom(buf, sizeof(buf), senderIp, sPort);

        if (len >= 20) {
            uint16_t type = ntohs(*(uint16_t*)buf);
            if (type == 0x0101) { // Binding Success
                // Reuse the parsing logic from GetPublicEndpoint or refactor
                // For brevity in this hardening phase, we'll implement a robust parse here
                int offset = 20;
                int payloadLen = ntohs(*(uint16_t*)(buf + 2));
                if (offset + payloadLen > len) payloadLen = len - offset;

                while (offset + 4 <= len) {
                    uint16_t attrType = ntohs(*(uint16_t*)(buf + offset));
                    uint16_t attrLen = ntohs(*(uint16_t*)(buf + offset + 2));
                    if (offset + 4 + attrLen > len) break;

                    if (attrType == 0x0001 && attrLen >= 8) { // MAPPED-ADDRESS
                        publicPort = ntohs(*(uint16_t*)(buf + offset + 6));
                        struct in_addr addr;
                        memcpy(&addr, buf + offset + 8, 4);
                        char ipBuf[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &addr, ipBuf, sizeof(ipBuf));
                        publicIp = ipBuf;
                        return true;
                    } else if (attrType == 0x0020 && attrLen >= 8) { // XOR-MAPPED-ADDRESS
                        publicPort = ntohs(*(uint16_t*)(buf + offset + 6)) ^ (0x2112A442 >> 16);
                        uint32_t xIp = *(uint32_t*)(buf + offset + 8) ^ htonl(0x2112A442);
                        struct in_addr addr;
                        addr.s_addr = xIp;
                        char ipBuf[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &addr, ipBuf, sizeof(ipBuf));
                        publicIp = ipBuf;
                        return true;
                    }
                    offset += 4 + attrLen;
                    if (attrLen % 4 != 0) offset += (4 - (attrLen % 4));
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;
}

} // namespace Network
