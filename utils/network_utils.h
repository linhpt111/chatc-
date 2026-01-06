#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")

namespace NetworkUtils {

// Initialize Winsock
inline bool initWinsock() {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

// Cleanup Winsock
inline void cleanupWinsock() {
    WSACleanup();
}

// Send a complete packet (header + payload)
inline bool sendPacket(SOCKET sock, PacketHeader* header, const char* payload, uint32_t payloadLen) {
    // Send header
    int sent = send(sock, (char*)header, sizeof(PacketHeader), 0);
    if (sent != sizeof(PacketHeader)) {
        return false;
    }
    
    // Send payload
    if (payloadLen > 0 && payload != nullptr) {
        sent = send(sock, payload, payloadLen, 0);
        if (sent != (int)payloadLen) {
            return false;
        }
    }
    
    return true;
}

// Receive complete payload
inline bool receivePayload(SOCKET sock, std::vector<char>& payload, uint32_t payloadLength) {
    payload.resize(payloadLength);
    int totalReceived = 0;
    
    while (totalReceived < (int)payloadLength) {
        int chunk = recv(sock, payload.data() + totalReceived, 
                        payloadLength - totalReceived, 0);
        if (chunk <= 0) {
            return false;
        }
        totalReceived += chunk;
    }
    
    return true;
}

// Send ACK message
inline void sendAck(SOCKET sock, const std::string& message) {
    PacketHeader ack = {0};
    ack.msgType = MSG_ACK;
    ack.payloadLength = message.length();
    
    send(sock, (char*)&ack, sizeof(PacketHeader), 0);
    if (!message.empty()) {
        send(sock, message.c_str(), message.length(), 0);
    }
}

// Send Error message
inline void sendError(SOCKET sock, const std::string& error) {
    PacketHeader err = {0};
    err.msgType = MSG_ERROR;
    err.payloadLength = error.length();
    
    send(sock, (char*)&err, sizeof(PacketHeader), 0);
    if (!error.empty()) {
        send(sock, error.c_str(), error.length(), 0);
    }
}

// Forward message to another socket
inline void forwardMessage(SOCKET targetSocket, PacketHeader* header, std::vector<char>& payload) {
    send(targetSocket, (char*)header, sizeof(PacketHeader), 0);
    if (header->payloadLength > 0) {
        send(targetSocket, payload.data(), payload.size(), 0);
    }
}

} // namespace NetworkUtils

#endif // NETWORK_UTILS_H
