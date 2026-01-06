#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

// Cross-platform socket support
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET SocketType;
    #define SOCKET_INVALID INVALID_SOCKET
    #define SOCKET_ERROR_CODE SOCKET_ERROR
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    typedef int SocketType;
    #define SOCKET_INVALID (-1)
    #define SOCKET_ERROR_CODE (-1)
    #define CLOSE_SOCKET(s) close(s)
    // For compatibility
    typedef int SOCKET;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
#endif

#include <string>
#include <vector>
#include "protocol.h"

namespace NetworkUtils {

// Initialize socket library (only needed on Windows)
inline bool initWinsock() {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true; // No initialization needed on Linux
#endif
}

// Cleanup socket library (only needed on Windows)
inline void cleanupWinsock() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// Send a complete packet (header + payload)
inline bool sendPacket(SocketType sock, PacketHeader* header, const char* payload, uint32_t payloadLen) {
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
inline bool receivePayload(SocketType sock, std::vector<char>& payload, uint32_t payloadLength) {
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
inline void sendAck(SocketType sock, const std::string& message) {
    PacketHeader ack = {0};
    ack.msgType = MSG_ACK;
    ack.payloadLength = message.length();
    
    send(sock, (char*)&ack, sizeof(PacketHeader), 0);
    if (!message.empty()) {
        send(sock, message.c_str(), message.length(), 0);
    }
}

// Send Error message
inline void sendError(SocketType sock, const std::string& error) {
    PacketHeader err = {0};
    err.msgType = MSG_ERROR;
    err.payloadLength = error.length();
    
    send(sock, (char*)&err, sizeof(PacketHeader), 0);
    if (!error.empty()) {
        send(sock, error.c_str(), error.length(), 0);
    }
}

// Forward message to another socket
inline void forwardMessage(SocketType targetSocket, PacketHeader* header, std::vector<char>& payload) {
    send(targetSocket, (char*)header, sizeof(PacketHeader), 0);
    if (header->payloadLength > 0) {
        send(targetSocket, payload.data(), payload.size(), 0);
    }
}

} // namespace NetworkUtils

#endif // NETWORK_UTILS_H
