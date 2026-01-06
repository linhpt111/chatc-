#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <map>
#include <string>
#include <mutex>
#include "../utils/network_utils.h"

class ClientManager {
private:
    std::map<std::string, SocketType> clients;      // username -> socket
    std::map<SocketType, std::string> socketToUser; // socket -> username
    std::mutex mtx;

public:
    ClientManager() = default;
    ~ClientManager() = default;

    // Add a new client
    bool addClient(const std::string& username, SocketType socket) {
        std::lock_guard<std::mutex> lock(mtx);
        
        if (clients.find(username) != clients.end()) {
            return false; // Username already exists
        }
        
        clients[username] = socket;
        socketToUser[socket] = username;
        return true;
    }

    // Remove a client by socket
    std::string removeClient(SocketType socket) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = socketToUser.find(socket);
        if (it == socketToUser.end()) {
            return "";
        }
        
        std::string username = it->second;
        clients.erase(username);
        socketToUser.erase(socket);
        
        return username;
    }

    // Get username by socket
    std::string getUsername(SocketType socket) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = socketToUser.find(socket);
        if (it != socketToUser.end()) {
            return it->second;
        }
        return "";
    }

    // Get socket by username
    SocketType getSocket(const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = clients.find(username);
        if (it != clients.end()) {
            return it->second;
        }
        return SOCKET_INVALID;
    }

    // Check if client exists
    bool exists(const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        return clients.find(username) != clients.end();
    }

    // Get all connected clients
    std::map<std::string, SocketType> getAllClients() {
        std::lock_guard<std::mutex> lock(mtx);
        return clients;
    }

    // Get client count
    size_t getClientCount() const {
        return clients.size();
    }
};

#endif // CLIENT_MANAGER_H
