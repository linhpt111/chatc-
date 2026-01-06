#ifndef BROKER_H
#define BROKER_H

#include "../utils/protocol.h"
#include "../utils/network_utils.h"
#include "../utils/database_manager.h"
#include "client_manager.h"
#include "topic_manager.h"
#include "file_transfer_manager.h"
#include "message_handler.h"
#include <iostream>
#include <thread>
#include <mutex>

class Broker {
private:
    SocketType serverSocket;
    ClientManager clientManager;
    TopicManager topicManager;
    FileTransferManager fileTransferManager;
    DatabaseManager* dbManager;
    MessageHandler* messageHandler;
    std::mutex mtx;
    bool running;

public:
    Broker() : serverSocket(SOCKET_INVALID), dbManager(nullptr), messageHandler(nullptr), running(false) {}
    
    ~Broker() {
        stop();
        delete messageHandler;
        delete dbManager;
    }
    
    bool initialize(int port = DEFAULT_PORT) {
        if (!NetworkUtils::initWinsock()) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }
        
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == SOCKET_INVALID) {
            std::cerr << "Socket creation failed" << std::endl;
            NetworkUtils::cleanupWinsock();
            return false;
        }
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);
        
        if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed" << std::endl;
            CLOSE_SOCKET(serverSocket);
            NetworkUtils::cleanupWinsock();
            return false;
        }
        
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed" << std::endl;
            CLOSE_SOCKET(serverSocket);
            NetworkUtils::cleanupWinsock();
            return false;
        }
        
        // Initialize database manager
        dbManager = new DatabaseManager("data");
        
        // Initialize message handler with database
        messageHandler = new MessageHandler(clientManager, topicManager, fileTransferManager, dbManager);
        
        std::cout << "[SERVER] Broker started on port " << port << std::endl;
        std::cout << "[SERVER] Database initialized in 'data/' folder" << std::endl;
        running = true;
        return true;
    }
    
    void run() {
        while (running) {
            SocketType clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket == SOCKET_INVALID) {
                if (running) {
                    std::cerr << "Accept failed" << std::endl;
                }
                continue;
            }
            
            std::cout << "[SERVER] New client connected" << std::endl;
            std::thread(&Broker::handleClient, this, clientSocket).detach();
        }
    }
    
    void stop() {
        running = false;
        if (serverSocket != SOCKET_INVALID) {
            CLOSE_SOCKET(serverSocket);
            serverSocket = SOCKET_INVALID;
        }
        NetworkUtils::cleanupWinsock();
    }
    
    // Get statistics
    size_t getClientCount() const { return clientManager.getClientCount(); }
    size_t getTopicCount() const { return topicManager.getTopicCount(); }
    size_t getActiveTransfers() const { return fileTransferManager.getActiveCount(); }

private:
    void handleClient(SocketType clientSocket) {
        char buffer[MAX_BUFFER_SIZE];
        
        while (running) {
            // Receive header
            int received = recv(clientSocket, buffer, sizeof(PacketHeader), 0);
            if (received <= 0) {
                messageHandler->handleDisconnect(clientSocket);
                break;
            }
            
            PacketHeader* header = (PacketHeader*)buffer;
            
            // Receive payload if any
            std::vector<char> payload;
            if (header->payloadLength > 0) {
                if (!NetworkUtils::receivePayload(clientSocket, payload, header->payloadLength)) {
                    messageHandler->handleDisconnect(clientSocket);
                    return;
                }
            }
            
            processMessage(clientSocket, header, payload);
        }
    }
    
    void processMessage(SocketType clientSocket, PacketHeader* header, std::vector<char>& payload) {
        std::lock_guard<std::mutex> lock(mtx);
        
        switch (header->msgType) {
            case MSG_LOGIN:
                messageHandler->handleLogin(clientSocket, header);
                break;
                
            case MSG_SUBSCRIBE:
                messageHandler->handleSubscribe(clientSocket, header);
                break;
                
            case MSG_UNSUBSCRIBE:
                messageHandler->handleUnsubscribe(clientSocket, header);
                break;
                
            case MSG_PUBLISH_TEXT:
                messageHandler->handlePublishText(clientSocket, header, payload);
                break;
                
            case MSG_PUBLISH_FILE:
                messageHandler->handlePublishFile(clientSocket, header, payload);
                break;
                
            case MSG_FILE_DATA:
                messageHandler->handleFileData(clientSocket, header, payload);
                break;
                
            case MSG_LOGOUT:
                messageHandler->handleDisconnect(clientSocket);
                break;
                
            case MSG_REQUEST_USER_LIST:
                messageHandler->handleRequestUserList(clientSocket);
                break;
                
            case MSG_REQUEST_HISTORY:
                messageHandler->handleRequestHistory(clientSocket, header, payload);
                break;
                
            case MSG_GAME:
                messageHandler->handleGameMessage(clientSocket, header, payload);
                break;
        }
    }
};

#endif // BROKER_H
