#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include "../utils/protocol.h"
#include "../utils/network_utils.h"
#include "../utils/string_utils.h"
#include "../utils/database_manager.h"
#include "client_manager.h"
#include "topic_manager.h"
#include "file_transfer_manager.h"
#include <iostream>
#include <vector>

class MessageHandler {
private:
    ClientManager& clientManager;
    TopicManager& topicManager;
    FileTransferManager& fileTransferManager;
    DatabaseManager* dbManager;

public:
    MessageHandler(ClientManager& cm, TopicManager& tm, FileTransferManager& ftm, DatabaseManager* db = nullptr)
        : clientManager(cm), topicManager(tm), fileTransferManager(ftm), dbManager(db) {}

    // Handle login message
    void handleLogin(SocketType clientSocket, PacketHeader* header) {
        std::string username(header->sender);
        
        if (clientManager.addClient(username, clientSocket)) {
            std::cout << "[LOGIN] User '" << username << "' logged in" << std::endl;
            
            // Save to database and set online
            if (dbManager) {
                dbManager->saveUser(username);
                dbManager->setUserOnline(username, true);
            }
            
            NetworkUtils::sendAck(clientSocket, "Login successful");
            
            // Broadcast user online to all clients
            broadcastUserStatus(username, true);
            
            // Send current online users list to this client
            sendUserList(clientSocket);
            
            // Send groups list to this client and auto-subscribe to joined groups
            sendGroupListAndSubscribe(clientSocket, username);
        } else {
            NetworkUtils::sendError(clientSocket, "Username already taken");
        }
    }

    // Handle subscribe message
    void handleSubscribe(SocketType clientSocket, PacketHeader* header) {
        std::string topic(header->topic);
        std::string username = clientManager.getUsername(clientSocket);
        
        if (topicManager.subscribe(topic, username)) {
            std::cout << "[SUBSCRIBE] User '" << username << "' subscribed to '" << topic << "'" << std::endl;
            
            // Save group to database
            if (dbManager && !StringUtils::isDMTopic(topic)) {
                bool isNewGroup = dbManager->saveGroup(topic, username);
                dbManager->addGroupMember(topic, username);
                
                // Only broadcast if it's a NEW group
                if (isNewGroup) {
                    broadcastNewGroup(topic, username);
                }
            }
            
            NetworkUtils::sendAck(clientSocket, "Subscribed to " + topic);
        }
    }

    // Handle unsubscribe message
    void handleUnsubscribe(SocketType clientSocket, PacketHeader* header) {
        std::string topic(header->topic);
        std::string username = clientManager.getUsername(clientSocket);
        
        topicManager.unsubscribe(topic, username);
        
        // Remove from database if it's a group (not DM)
        if (dbManager && !StringUtils::isDMTopic(topic)) {
            dbManager->removeGroupMember(topic, username);
        }
        
        std::cout << "[UNSUBSCRIBE] User '" << username << "' unsubscribed from '" << topic << "'" << std::endl;
        NetworkUtils::sendAck(clientSocket, "Unsubscribed from " + topic);
    }

    // Handle text message publish
    void handlePublishText(SocketType clientSocket, PacketHeader* header, std::vector<char>& payload) {
        std::string topic(header->topic);
        std::string sender(header->sender);
        std::string message(payload.begin(), payload.end());
        
        std::cout << "[PUBLISH] User '" << sender << "' published to '" << topic << "'" << std::endl;
        
        // Save message to database
        if (dbManager) {
            if (StringUtils::isDMTopic(topic)) {
                std::string recipient = StringUtils::extractRecipient(topic, sender);
                dbManager->saveMessage(sender, recipient, message, false);
            } else {
                dbManager->saveMessage(sender, topic, message, true);
            }
        }
        
        if (StringUtils::isDMTopic(topic)) {
            // Direct message - send to recipient only
            std::string recipient = StringUtils::extractRecipient(topic, sender);
            SocketType recipientSocket = clientManager.getSocket(recipient);
            if (recipientSocket != SOCKET_INVALID) {
                NetworkUtils::forwardMessage(recipientSocket, header, payload);
            }
        } else {
            // Group message - send to all subscribers
            auto subscribers = topicManager.getSubscribers(topic);
            for (const std::string& subscriber : subscribers) {
                if (subscriber != sender) {
                    SocketType subscriberSocket = clientManager.getSocket(subscriber);
                    if (subscriberSocket != SOCKET_INVALID) {
                        NetworkUtils::forwardMessage(subscriberSocket, header, payload);
                    }
                }
            }
        }
        
        NetworkUtils::sendAck(clientSocket, "Message published");
    }

    // Handle file metadata
    void handlePublishFile(SocketType clientSocket, PacketHeader* header, std::vector<char>& payload) {
        std::string topic(header->topic);
        std::string sender(header->sender);
        
        // Extract filename and size from payload
        uint32_t filenameLen = *(uint32_t*)payload.data();
        std::string filename(payload.data() + 4, filenameLen);
        uint32_t fileSize = *(uint32_t*)(payload.data() + 4 + filenameLen);
        
        std::cout << "[FILE] User '" << sender << "' sending file '" << filename 
                  << "' (" << fileSize << " bytes) to '" << topic << "'" << std::endl;
        
        // Start file transfer tracking
        fileTransferManager.startTransfer(header->messageId, filename, fileSize, sender, topic);
        
        // Forward file metadata to recipients
        if (StringUtils::isDMTopic(topic)) {
            std::string recipient = StringUtils::extractRecipient(topic, sender);
            SocketType recipientSocket = clientManager.getSocket(recipient);
            if (recipientSocket != SOCKET_INVALID) {
                NetworkUtils::forwardMessage(recipientSocket, header, payload);
            }
        } else {
            auto subscribers = topicManager.getSubscribers(topic);
            for (const std::string& subscriber : subscribers) {
                if (subscriber != sender) {
                    SocketType subscriberSocket = clientManager.getSocket(subscriber);
                    if (subscriberSocket != SOCKET_INVALID) {
                        NetworkUtils::forwardMessage(subscriberSocket, header, payload);
                    }
                }
            }
        }
        
        NetworkUtils::sendAck(clientSocket, "Ready to receive file");
    }

    // Handle file data chunk
    void handleFileData(SocketType clientSocket, PacketHeader* header, std::vector<char>& payload) {
        uint32_t msgId = header->messageId;
        
        if (!fileTransferManager.exists(msgId)) {
            NetworkUtils::sendError(clientSocket, "No active file transfer");
            return;
        }
        
        fileTransferManager.addChunk(msgId, payload);
        
        float progress = fileTransferManager.getProgress(msgId);
        std::cout << "[FILE DATA] Progress: " << (int)(progress * 100) << "%" << std::endl;
        
        // Forward chunk to recipients
        std::string topic = fileTransferManager.getRecipient(msgId);
        std::string sender = fileTransferManager.getSender(msgId);
        
        if (StringUtils::isDMTopic(topic)) {
            std::string recipient = StringUtils::extractRecipient(topic, sender);
            SocketType recipientSocket = clientManager.getSocket(recipient);
            if (recipientSocket != SOCKET_INVALID) {
                NetworkUtils::forwardMessage(recipientSocket, header, payload);
            }
        } else {
            auto subscribers = topicManager.getSubscribers(topic);
            for (const std::string& subscriber : subscribers) {
                if (subscriber != sender) {
                    SocketType subscriberSocket = clientManager.getSocket(subscriber);
                    if (subscriberSocket != SOCKET_INVALID) {
                        NetworkUtils::forwardMessage(subscriberSocket, header, payload);
                    }
                }
            }
        }
        
        // Cleanup if complete
        if (fileTransferManager.isComplete(msgId)) {
            std::cout << "[FILE] Transfer complete" << std::endl;
            fileTransferManager.removeTransfer(msgId);
            NetworkUtils::sendAck(clientSocket, "File transfer complete");
        }
        // Don't send ACK for each chunk - only when complete
    }

    // Handle client disconnect
    void handleDisconnect(SocketType clientSocket) {
        std::string username = clientManager.removeClient(clientSocket);
        
        if (!username.empty()) {
            topicManager.removeUserFromAllTopics(username);
            
            // Update database
            if (dbManager) {
                dbManager->setUserOnline(username, false);
            }
            
            std::cout << "[LOGOUT] User '" << username << "' disconnected" << std::endl;
            
            // Broadcast user offline to all clients
            broadcastUserStatus(username, false);
        }
        
        CLOSE_SOCKET(clientSocket);
    }
    
    // Handle request for online users list
    void handleRequestUserList(SocketType clientSocket) {
        sendUserList(clientSocket);
    }
    
    // Handle request for chat history
    void handleRequestHistory(SocketType clientSocket, PacketHeader* header, std::vector<char>& payload) {
        if (!dbManager) return;
        
        std::string topic(header->topic);
        std::string username = clientManager.getUsername(clientSocket);
        
        std::vector<ChatMessage> history;
        
        if (StringUtils::isDMTopic(topic)) {
            std::string otherUser = StringUtils::extractRecipient(topic, username);
            history = dbManager->getDirectMessageHistory(username, otherUser, 50);
        } else {
            history = dbManager->getMessageHistory(topic, 50);
        }
        
        // Send history messages
        for (const auto& msg : history) {
            PacketHeader histHeader = {0};
            histHeader.msgType = MSG_HISTORY_DATA;
            histHeader.timestamp = msg.timestamp;
            strncpy(histHeader.sender, msg.sender.c_str(), MAX_USERNAME_LEN - 1);
            strncpy(histHeader.topic, topic.c_str(), MAX_TOPIC_LEN - 1);
            
            std::string content = msg.content;
            if (msg.isFile) {
                content = "[FILE] " + msg.filename;
            }
            
            histHeader.payloadLength = content.length();
            
            std::vector<char> histPayload(content.begin(), content.end());
            NetworkUtils::forwardMessage(clientSocket, &histHeader, histPayload);
        }
        
        NetworkUtils::sendAck(clientSocket, "History sent");
    }
    
    // Handle game message - just forward to recipient
    void handleGameMessage(SocketType clientSocket, PacketHeader* header, std::vector<char>& payload) {
        std::string sender(header->sender);
        std::string recipient(header->topic);  // topic contains recipient username
        
        std::cout << "[GAME] From '" << sender << "' to '" << recipient << "'" << std::endl;
        
        // Forward to recipient
        SocketType recipientSocket = clientManager.getSocket(recipient);
        if (recipientSocket != SOCKET_INVALID) {
            NetworkUtils::forwardMessage(recipientSocket, header, payload);
        }
    }

private:
    // Broadcast user online/offline status to all connected clients
    void broadcastUserStatus(const std::string& username, bool online) {
        PacketHeader header = {0};
        header.msgType = online ? MSG_USER_ONLINE : MSG_USER_OFFLINE;
        header.payloadLength = username.length();
        header.timestamp = time(nullptr);
        strncpy(header.sender, username.c_str(), MAX_USERNAME_LEN - 1);
        
        std::vector<char> payload(username.begin(), username.end());
        
        auto clients = clientManager.getAllClients();
        for (const auto& client : clients) {
            if (client.first != username) {
                NetworkUtils::forwardMessage(client.second, &header, payload);
            }
        }
        
        std::cout << "[STATUS] User '" << username << "' is now " 
                  << (online ? "ONLINE" : "OFFLINE") << std::endl;
    }
    
    // Send list of online users to a specific client (excluding themselves)
    void sendUserList(SocketType clientSocket) {
        std::string currentUser = clientManager.getUsername(clientSocket);
        auto clients = clientManager.getAllClients();
        
        // Build user list as semicolon-separated string, excluding current user
        std::string userList;
        for (const auto& client : clients) {
            if (client.first != currentUser) {
                if (!userList.empty()) userList += ";";
                userList += client.first;
            }
        }
        
        PacketHeader header = {0};
        header.msgType = MSG_USER_LIST;
        header.payloadLength = userList.length();
        header.timestamp = time(nullptr);
        
        std::vector<char> payload(userList.begin(), userList.end());
        NetworkUtils::forwardMessage(clientSocket, &header, payload);
        
        std::cout << "[USER LIST] Sent to " << currentUser << ": " << userList << std::endl;
    }
    
    // Broadcast new group creation to all connected clients
    void broadcastNewGroup(const std::string& groupName, const std::string& creator) {
        PacketHeader header = {0};
        header.msgType = MSG_GROUP_CREATED;
        header.payloadLength = groupName.length();
        header.timestamp = time(nullptr);
        strncpy(header.sender, creator.c_str(), MAX_USERNAME_LEN - 1);
        strncpy(header.topic, groupName.c_str(), MAX_TOPIC_LEN - 1);
        
        std::vector<char> payload(groupName.begin(), groupName.end());
        
        auto clients = clientManager.getAllClients();
        for (const auto& client : clients) {
            NetworkUtils::forwardMessage(client.second, &header, payload);
        }
        
        std::cout << "[GROUP] Broadcast new group '" << groupName << "' created by " << creator << std::endl;
    }
    
    // Send list of all groups with membership info to a specific client
    void sendGroupList(SocketType clientSocket, const std::string& username) {
        if (!dbManager) return;
        
        auto groups = dbManager->getAllGroupsWithMembership(username);
        
        // Format: groupName:1;groupName2:0;... (1=member, 0=not member)
        std::string groupList;
        for (const auto& g : groups) {
            if (!groupList.empty()) groupList += ";";
            groupList += g.first + ":" + (g.second ? "1" : "0");
        }
        
        PacketHeader header = {0};
        header.msgType = MSG_GROUP_LIST;
        header.payloadLength = groupList.length();
        header.timestamp = time(nullptr);
        
        std::vector<char> payload(groupList.begin(), groupList.end());
        NetworkUtils::forwardMessage(clientSocket, &header, payload);
        
        std::cout << "[GROUP LIST] Sent to " << username << ": " << groupList << std::endl;
    }
    
    // Send list of all groups AND auto-subscribe user to their joined groups
    void sendGroupListAndSubscribe(SocketType clientSocket, const std::string& username) {
        if (!dbManager) return;
        
        auto groups = dbManager->getAllGroupsWithMembership(username);
        
        // Format: groupName:1;groupName2:0;... (1=member, 0=not member)
        std::string groupList;
        for (const auto& g : groups) {
            if (!groupList.empty()) groupList += ";";
            groupList += g.first + ":" + (g.second ? "1" : "0");
            
            // Auto-subscribe to groups user is a member of
            if (g.second) {
                topicManager.subscribe(g.first, username);
                std::cout << "[AUTO-SUBSCRIBE] User '" << username << "' subscribed to group '" << g.first << "'" << std::endl;
            }
        }
        
        PacketHeader header = {0};
        header.msgType = MSG_GROUP_LIST;
        header.payloadLength = groupList.length();
        header.timestamp = time(nullptr);
        
        std::vector<char> payload(groupList.begin(), groupList.end());
        NetworkUtils::forwardMessage(clientSocket, &header, payload);
        
        std::cout << "[GROUP LIST] Sent to " << username << ": " << groupList << std::endl;
    }
};

#endif // MESSAGE_HANDLER_H
