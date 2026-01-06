#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <map>
#include <string>
#include <mutex>
#include <winsock2.h>

class ClientManager {
private:
    std::map<std::string, SOCKET> clients;      // username -> socket
    std::map<SOCKET, std::string> socketToUser; // socket -> username
    std::mutex mtx;

public:
    ClientManager() = default;
    ~ClientManager() = default;

    // Add a new client
    bool addClient(const std::string& username, SOCKET socket) {
        std::lock_guard<std::mutex> lock(mtx);
        
        if (clients.find(username) != clients.end()) {
            return false; // Username already exists
        }
        
        clients[username] = socket;
        socketToUser[socket] = username;
        return true;
    }

    // Remove a client by socket
    std::string removeClient(SOCKET socket) {
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
    std::string getUsername(SOCKET socket) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = socketToUser.find(socket);
        if (it != socketToUser.end()) {
            return it->second;
        }
        return "";
    }

    // Get socket by username
    SOCKET getSocket(const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = clients.find(username);
        if (it != clients.end()) {
            return it->second;
        }
        return INVALID_SOCKET;
    }

    // Check if client exists
    bool exists(const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        return clients.find(username) != clients.end();
    }

    // Get all connected clients
    std::map<std::string, SOCKET> getAllClients() {
        std::lock_guard<std::mutex> lock(mtx);
        return clients;
    }

    // Get client count
    size_t getClientCount() const {
        return clients.size();
    }
};

#endif // CLIENT_MANAGER_H
