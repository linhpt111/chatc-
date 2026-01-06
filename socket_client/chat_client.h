#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include "../utils/protocol.h"
#include "../utils/network_utils.h"
#include "../utils/string_utils.h"
#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <fstream>
#include <functional>

// Cross-platform includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <direct.h>
    #define MKDIR(dir) _mkdir(dir)
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #define MKDIR(dir) mkdir(dir, 0755)
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

class ChatClient {
public:
    using MessageCallback = std::function<void(const std::string&, const std::string&, const std::string&)>;
    using FileCallback = std::function<void(const std::string&, const std::string&, uint32_t)>;
    using UserStatusCallback = std::function<void(const std::string&, bool)>;  // username, isOnline
    using UserListCallback = std::function<void(const std::vector<std::string>&)>;
    using HistoryCallback = std::function<void(const std::string&, const std::string&, const std::string&, time_t)>;
    using GroupCallback = std::function<void(const std::string&, const std::string&)>;  // groupName, creator
    using GroupListCallback = std::function<void(const std::vector<std::pair<std::string, bool>>&)>;  // groupName, isMember
    using GameCallback = std::function<void(const std::string&, const std::string&)>;  // from, payload

private:
    SocketType clientSocket;
    std::string username;
    std::string currentTopic;
    bool connected;
    std::mutex mtx;
    std::vector<std::string> onlineUsers;
    
    MessageCallback onMessageReceived;
    FileCallback onFileReceived;
    UserStatusCallback onUserStatusChanged;
    UserListCallback onUserListReceived;
    HistoryCallback onHistoryReceived;
    GroupCallback onGroupCreated;
    GroupListCallback onGroupListReceived;
    GameCallback onGameReceived;
    
    struct FileReceiver {
        std::string filename;
        uint32_t fileSize;
        uint32_t receivedSize;
        std::ofstream file;
        std::string sender;
    };
    
    std::map<uint32_t, FileReceiver> activeDownloads;

public:
    ChatClient() : clientSocket(SOCKET_INVALID), connected(false) {}
    
    ~ChatClient() {
        disconnect();
    }
    
    void setMessageCallback(MessageCallback callback) {
        onMessageReceived = callback;
    }
    
    void setFileCallback(FileCallback callback) {
        onFileReceived = callback;
    }
    
    void setUserStatusCallback(UserStatusCallback callback) {
        onUserStatusChanged = callback;
    }
    
    void setUserListCallback(UserListCallback callback) {
        onUserListReceived = callback;
    }
    
    void setHistoryCallback(HistoryCallback callback) {
        onHistoryReceived = callback;
    }
    
    void setGroupCallback(GroupCallback callback) {
        onGroupCreated = callback;
    }
    
    void setGroupListCallback(GroupListCallback callback) {
        onGroupListReceived = callback;
    }
    
    void setGameCallback(GameCallback callback) {
        onGameReceived = callback;
    }
    
    std::vector<std::string> getOnlineUsers() const {
        return onlineUsers;
    }
    
    bool connect(const std::string& serverIp, int port, const std::string& user) {
        if (!NetworkUtils::initWinsock()) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }
        
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == SOCKET_INVALID) {
            std::cerr << "Socket creation failed" << std::endl;
            NetworkUtils::cleanupWinsock();
            return false;
        }
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr);
        
        if (::connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connection failed" << std::endl;
            CLOSE_SOCKET(clientSocket);
            NetworkUtils::cleanupWinsock();
            return false;
        }
        
        username = user;
        connected = true;
        
        if (!sendLogin()) {
            disconnect();
            return false;
        }
        
        std::thread(&ChatClient::receiveLoop, this).detach();
        
        std::cout << "[CLIENT] Connected as '" << username << "'" << std::endl;
        return true;
    }
    
    void disconnect() {
        if (connected) {
            sendLogout();
            connected = false;
        }
        
        if (clientSocket != SOCKET_INVALID) {
            CLOSE_SOCKET(clientSocket);
            clientSocket = SOCKET_INVALID;
        }
        
        NetworkUtils::cleanupWinsock();
    }
    
    bool isConnected() const { return connected; }
    std::string getUsername() const { return username; }
    
    bool requestUserList() {
        PacketHeader header = {0};
        header.msgType = MSG_REQUEST_USER_LIST;
        header.payloadLength = 0;
        strncpy(header.sender, username.c_str(), MAX_USERNAME_LEN - 1);
        
        return sendPacket(&header, nullptr, 0);
    }
    
    bool requestHistory(const std::string& topic) {
        PacketHeader header = {0};
        header.msgType = MSG_REQUEST_HISTORY;
        header.payloadLength = 0;
        strncpy(header.sender, username.c_str(), MAX_USERNAME_LEN - 1);
        strncpy(header.topic, topic.c_str(), MAX_TOPIC_LEN - 1);
        
        return sendPacket(&header, nullptr, 0);
    }
    
    bool joinGroup(const std::string& groupName) {
        return subscribe(groupName);
    }
    
    bool leaveGroup(const std::string& groupName) {
        return unsubscribe(groupName);
    }
    
    bool sendDirectMessage(const std::string& recipient, const std::string& message) {
        std::string topic = StringUtils::createDMTopic(username, recipient);
        return publishText(topic, message);
    }
    
    bool sendGroupMessage(const std::string& groupName, const std::string& message) {
        return publishText(groupName, message);
    }
    
    bool sendFileToUser(const std::string& recipient, const std::string& filepath) {
        std::string topic = StringUtils::createDMTopic(username, recipient);
        return sendFile(topic, filepath);
    }
    
    bool sendFileToGroup(const std::string& groupName, const std::string& filepath) {
        return sendFile(groupName, filepath);
    }
    
    bool sendGameMessage(const std::string& recipient, const std::string& payload) {
        PacketHeader header = {0};
        header.msgType = MSG_GAME;
        header.payloadLength = payload.length();
        header.messageId = rand();
        header.timestamp = time(nullptr);
        strncpy(header.sender, username.c_str(), MAX_USERNAME_LEN - 1);
        strncpy(header.topic, recipient.c_str(), MAX_TOPIC_LEN - 1);
        
        return sendPacket(&header, payload.c_str(), payload.length());
    }
    
    bool subscribe(const std::string& topic) {
        PacketHeader header = {0};
        header.msgType = MSG_SUBSCRIBE;
        header.payloadLength = 0;
        strncpy(header.sender, username.c_str(), MAX_USERNAME_LEN - 1);
        strncpy(header.topic, topic.c_str(), MAX_TOPIC_LEN - 1);
        
        return sendPacket(&header, nullptr, 0);
    }
    
    bool unsubscribe(const std::string& topic) {
        PacketHeader header = {0};
        header.msgType = MSG_UNSUBSCRIBE;
        header.payloadLength = 0;
        strncpy(header.sender, username.c_str(), MAX_USERNAME_LEN - 1);
        strncpy(header.topic, topic.c_str(), MAX_TOPIC_LEN - 1);
        
        return sendPacket(&header, nullptr, 0);
    }

private:
    bool sendLogin() {
        PacketHeader header = {0};
        header.msgType = MSG_LOGIN;
        header.payloadLength = 0;
        strncpy(header.sender, username.c_str(), MAX_USERNAME_LEN - 1);
        
        return sendPacket(&header, nullptr, 0);
    }
    
    bool sendLogout() {
        PacketHeader header = {0};
        header.msgType = MSG_LOGOUT;
        header.payloadLength = 0;
        strncpy(header.sender, username.c_str(), MAX_USERNAME_LEN - 1);
        
        return sendPacket(&header, nullptr, 0);
    }
    
    bool publishText(const std::string& topic, const std::string& message) {
        PacketHeader header = {0};
        header.msgType = MSG_PUBLISH_TEXT;
        header.payloadLength = message.length();
        header.messageId = rand();
        header.timestamp = time(nullptr);
        strncpy(header.sender, username.c_str(), MAX_USERNAME_LEN - 1);
        strncpy(header.topic, topic.c_str(), MAX_TOPIC_LEN - 1);
        
        return sendPacket(&header, message.c_str(), message.length());
    }
    
    bool sendFile(const std::string& topic, const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filepath << std::endl;
            return false;
        }
        
        uint32_t fileSize = file.tellg();
        file.seekg(0);
        
        size_t pos = filepath.find_last_of("\\/");
        std::string filename = (pos != std::string::npos) ? filepath.substr(pos + 1) : filepath;
        
        PacketHeader header = {0};
        header.msgType = MSG_PUBLISH_FILE;
        header.messageId = rand();
        header.timestamp = time(nullptr);
        strncpy(header.sender, username.c_str(), MAX_USERNAME_LEN - 1);
        strncpy(header.topic, topic.c_str(), MAX_TOPIC_LEN - 1);
        
        std::vector<char> metadata;
        uint32_t filenameLen = filename.length();
        metadata.insert(metadata.end(), (char*)&filenameLen, (char*)&filenameLen + 4);
        metadata.insert(metadata.end(), filename.begin(), filename.end());
        metadata.insert(metadata.end(), (char*)&fileSize, (char*)&fileSize + 4);
        
        header.payloadLength = metadata.size();
        
        if (!sendPacket(&header, metadata.data(), metadata.size())) {
            file.close();
            return false;
        }
        
        std::vector<char> buffer(FILE_CHUNK_SIZE);
        uint32_t totalSent = 0;
        
        while (totalSent < fileSize) {
            uint32_t chunkSize = std::min((uint32_t)FILE_CHUNK_SIZE, fileSize - totalSent);
            file.read(buffer.data(), chunkSize);
            
            PacketHeader chunkHeader = {0};
            chunkHeader.msgType = MSG_FILE_DATA;
            chunkHeader.messageId = header.messageId;
            chunkHeader.payloadLength = chunkSize;
            strncpy(chunkHeader.sender, username.c_str(), MAX_USERNAME_LEN - 1);
            strncpy(chunkHeader.topic, topic.c_str(), MAX_TOPIC_LEN - 1);
            
            if (!sendPacket(&chunkHeader, buffer.data(), chunkSize)) {
                file.close();
                return false;
            }
            
            totalSent += chunkSize;
            
            // Small delay to prevent network flooding
            SLEEP_MS(1);
        }
        
        file.close();
        std::cout << "[FILE] Transfer complete: " << filename << std::endl;
        return true;
    }
    
    bool sendPacket(PacketHeader* header, const char* payload, uint32_t payloadLen) {
        std::lock_guard<std::mutex> lock(mtx);
        return NetworkUtils::sendPacket(clientSocket, header, payload, payloadLen);
    }
    
    void receiveLoop() {
        char buffer[MAX_BUFFER_SIZE];
        
        while (connected) {
            int received = recv(clientSocket, buffer, sizeof(PacketHeader), 0);
            if (received <= 0) {
                connected = false;
                std::cout << "[CLIENT] Disconnected from server" << std::endl;
                break;
            }
            
            PacketHeader* header = (PacketHeader*)buffer;
            
            std::vector<char> payload;
            if (header->payloadLength > 0) {
                if (!NetworkUtils::receivePayload(clientSocket, payload, header->payloadLength)) {
                    connected = false;
                    return;
                }
            }
            
            handleMessage(header, payload);
        }
    }
    
    void handleMessage(PacketHeader* header, std::vector<char>& payload) {
        switch (header->msgType) {
            case MSG_PUBLISH_TEXT:
                handleTextMessage(header, payload);
                break;
                
            case MSG_PUBLISH_FILE:
                handleFileMetadata(header, payload);
                break;
                
            case MSG_FILE_DATA:
                handleFileData(header, payload);
                break;
                
            case MSG_ACK:
                handleAck(payload);
                break;
                
            case MSG_ERROR:
                handleError(payload);
                break;
                
            case MSG_USER_ONLINE:
                handleUserOnline(header, payload);
                break;
                
            case MSG_USER_OFFLINE:
                handleUserOffline(header, payload);
                break;
                
            case MSG_USER_LIST:
                handleUserList(payload);
                break;
                
            case MSG_HISTORY_DATA:
                handleHistoryData(header, payload);
                break;
                
            case MSG_GROUP_CREATED:
                handleGroupCreated(header, payload);
                break;
                
            case MSG_GROUP_LIST:
                handleGroupList(payload);
                break;
                
            case MSG_GAME:
                handleGameMessage(header, payload);
                break;
        }
    }
    
    void handleGameMessage(PacketHeader* header, std::vector<char>& payload) {
        std::string from(header->sender);
        std::string gamePayload(payload.begin(), payload.end());
        
        std::cout << "[GAME] From " << from << ": " << gamePayload << std::endl;
        
        if (onGameReceived) {
            onGameReceived(from, gamePayload);
        }
    }
    
    void handleTextMessage(PacketHeader* header, std::vector<char>& payload) {
        std::string sender(header->sender);
        std::string topic(header->topic);
        std::string message(payload.begin(), payload.end());
        
        std::cout << "[" << topic << "] " << sender << ": " << message << std::endl;
        
        if (onMessageReceived) {
            onMessageReceived(sender, topic, message);
        }
    }
    
    void handleFileMetadata(PacketHeader* header, std::vector<char>& payload) {
        uint32_t filenameLen = *(uint32_t*)payload.data();
        std::string filename(payload.data() + 4, filenameLen);
        uint32_t fileSize = *(uint32_t*)(payload.data() + 4 + filenameLen);
        std::string sender(header->sender);
        
        std::cout << "[FILE] Receiving '" << filename << "' (" << fileSize 
                  << " bytes) from " << sender << std::endl;
        
        MKDIR("downloads");
        
        FileReceiver fr;
        fr.filename = filename;
        fr.fileSize = fileSize;
        fr.receivedSize = 0;
        fr.sender = sender;
#ifdef _WIN32
        fr.file.open("downloads\\" + filename, std::ios::binary);
#else
        fr.file.open("downloads/" + filename, std::ios::binary);
#endif
        
        activeDownloads[header->messageId] = std::move(fr);
    }
    
    void handleFileData(PacketHeader* header, std::vector<char>& payload) {
        uint32_t msgId = header->messageId;
        
        if (activeDownloads.find(msgId) == activeDownloads.end()) {
            std::cerr << "[FILE] Unknown file transfer" << std::endl;
            return;
        }
        
        FileReceiver& fr = activeDownloads[msgId];
        fr.file.write(payload.data(), payload.size());
        fr.receivedSize += payload.size();
        
        std::cout << "[FILE] Received " << fr.receivedSize << "/" << fr.fileSize << " bytes" << std::endl;
        
        if (fr.receivedSize >= fr.fileSize) {
            fr.file.close();
            std::cout << "[FILE] Download complete: " << fr.filename << std::endl;
            
            if (onFileReceived) {
                onFileReceived(fr.sender, fr.filename, fr.fileSize);
            }
            
            activeDownloads.erase(msgId);
        }
    }
    
    void handleAck(std::vector<char>& payload) {
        if (!payload.empty()) {
            std::string message(payload.begin(), payload.end());
            std::cout << "[ACK] " << message << std::endl;
        }
    }
    
    void handleError(std::vector<char>& payload) {
        if (!payload.empty()) {
            std::string error(payload.begin(), payload.end());
            std::cerr << "[ERROR] " << error << std::endl;
        }
    }
    
    void handleUserOnline(PacketHeader* header, std::vector<char>& payload) {
        std::string user(payload.begin(), payload.end());
        
        // Add to online users list if not already present
        auto it = std::find(onlineUsers.begin(), onlineUsers.end(), user);
        if (it == onlineUsers.end()) {
            onlineUsers.push_back(user);
        }
        
        std::cout << "[STATUS] " << user << " is now ONLINE" << std::endl;
        
        if (onUserStatusChanged) {
            onUserStatusChanged(user, true);
        }
    }
    
    void handleUserOffline(PacketHeader* header, std::vector<char>& payload) {
        std::string user(payload.begin(), payload.end());
        
        // Remove from online users list
        auto it = std::find(onlineUsers.begin(), onlineUsers.end(), user);
        if (it != onlineUsers.end()) {
            onlineUsers.erase(it);
        }
        
        std::cout << "[STATUS] " << user << " is now OFFLINE" << std::endl;
        
        if (onUserStatusChanged) {
            onUserStatusChanged(user, false);
        }
    }
    
    void handleUserList(std::vector<char>& payload) {
        std::string userListStr(payload.begin(), payload.end());
        
        onlineUsers.clear();
        
        // Parse semicolon-separated user list
        std::istringstream iss(userListStr);
        std::string user;
        while (std::getline(iss, user, ';')) {
            if (!user.empty()) {
                onlineUsers.push_back(user);
            }
        }
        
        std::cout << "[USER LIST] Online users: ";
        for (const auto& u : onlineUsers) {
            std::cout << u << " ";
        }
        std::cout << std::endl;
        
        if (onUserListReceived) {
            onUserListReceived(onlineUsers);
        }
    }
    
    void handleHistoryData(PacketHeader* header, std::vector<char>& payload) {
        std::string sender(header->sender);
        std::string topic(header->topic);
        std::string message(payload.begin(), payload.end());
        time_t timestamp = header->timestamp;
        
        std::cout << "[HISTORY] [" << topic << "] " << sender << ": " << message << std::endl;
        
        if (onHistoryReceived) {
            onHistoryReceived(sender, topic, message, timestamp);
        }
    }
    
    void handleGroupCreated(PacketHeader* header, std::vector<char>& payload) {
        std::string groupName(payload.begin(), payload.end());
        std::string creator(header->sender);
        
        std::cout << "[GROUP] New group '" << groupName << "' created by " << creator << std::endl;
        
        if (onGroupCreated) {
            onGroupCreated(groupName, creator);
        }
    }
    
    void handleGroupList(std::vector<char>& payload) {
        std::string groupListStr(payload.begin(), payload.end());
        
        std::vector<std::pair<std::string, bool>> groups;
        
        // Parse format: groupName:1;groupName2:0;...
        std::istringstream iss(groupListStr);
        std::string item;
        while (std::getline(iss, item, ';')) {
            if (!item.empty()) {
                size_t colonPos = item.find(':');
                if (colonPos != std::string::npos) {
                    std::string groupName = item.substr(0, colonPos);
                    bool isMember = (item.substr(colonPos + 1) == "1");
                    groups.push_back({groupName, isMember});
                }
            }
        }
        
        std::cout << "[GROUP LIST] Received " << groups.size() << " groups" << std::endl;
        
        if (onGroupListReceived) {
            onGroupListReceived(groups);
        }
    }
};

#endif // CHAT_CLIENT_H
