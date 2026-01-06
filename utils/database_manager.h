#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <vector>
#include <ctime>
#include <fstream>
#include <sstream>
#include <mutex>
#include <map>
#include <algorithm>

// Simple file-based database (no external dependencies)
// Format: CSV files for each table

struct ChatMessage {
    uint32_t id;
    std::string sender;
    std::string recipient; // username or group name
    std::string content;
    uint64_t timestamp;
    bool isGroup;
    bool isFile;
    std::string filename;
};

struct UserRecord {
    std::string username;
    std::string passwordHash; // For future authentication
    uint64_t createdAt;
    uint64_t lastSeen;
    bool isOnline;
};

struct GroupRecord {
    std::string groupName;
    std::string createdBy;
    uint64_t createdAt;
    std::vector<std::string> members;
};

class DatabaseManager {
private:
    std::string dataDir;
    std::mutex mtx;
    uint32_t nextMessageId;
    
    std::string messagesFile;
    std::string usersFile;
    std::string groupsFile;

public:
    DatabaseManager(const std::string& directory = "data") 
        : dataDir(directory), nextMessageId(1) {
        
        messagesFile = dataDir + "/messages.csv";
        usersFile = dataDir + "/users.csv";
        groupsFile = dataDir + "/groups.csv";
        
        // Create data directory
        createDirectory(dataDir);
        
        // Initialize files if they don't exist
        initializeFiles();
        
        // Load next message ID
        loadNextMessageId();
    }
    
    // ============ Messages ============
    
    bool saveMessage(const std::string& sender, const std::string& recipient,
                     const std::string& content, bool isGroup, 
                     bool isFile = false, const std::string& filename = "") {
        std::lock_guard<std::mutex> lock(mtx);
        
        std::ofstream file(messagesFile, std::ios::app);
        if (!file.is_open()) return false;
        
        uint64_t timestamp = time(nullptr);
        
        // CSV format: id,sender,recipient,content,timestamp,isGroup,isFile,filename
        file << nextMessageId++ << ","
             << escapeCSV(sender) << ","
             << escapeCSV(recipient) << ","
             << escapeCSV(content) << ","
             << timestamp << ","
             << (isGroup ? "1" : "0") << ","
             << (isFile ? "1" : "0") << ","
             << escapeCSV(filename) << "\n";
        
        file.close();
        return true;
    }
    
    std::vector<ChatMessage> getMessageHistory(const std::string& topic, int limit = 50) {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<ChatMessage> messages;
        
        std::ifstream file(messagesFile);
        if (!file.is_open()) return messages;
        
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            ChatMessage msg = parseMessage(line);
            if (msg.recipient == topic) {
                messages.push_back(msg);
            }
        }
        
        // Return last 'limit' messages
        if (messages.size() > (size_t)limit) {
            messages.erase(messages.begin(), messages.end() - limit);
        }
        
        return messages;
    }
    
    std::vector<ChatMessage> getDirectMessageHistory(const std::string& user1, 
                                                      const std::string& user2, 
                                                      int limit = 50) {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<ChatMessage> messages;
        
        std::ifstream file(messagesFile);
        if (!file.is_open()) return messages;
        
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            ChatMessage msg = parseMessage(line);
            if (!msg.isGroup) {
                if ((msg.sender == user1 && msg.recipient == user2) ||
                    (msg.sender == user2 && msg.recipient == user1)) {
                    messages.push_back(msg);
                }
            }
        }
        
        if (messages.size() > (size_t)limit) {
            messages.erase(messages.begin(), messages.end() - limit);
        }
        
        return messages;
    }
    
    // ============ Users ============
    
    bool saveUser(const std::string& username, const std::string& passwordHash = "") {
        std::lock_guard<std::mutex> lock(mtx);
        
        // Check if user exists
        if (userExists(username)) {
            return updateLastSeen(username);
        }
        
        std::ofstream file(usersFile, std::ios::app);
        if (!file.is_open()) return false;
        
        uint64_t now = time(nullptr);
        
        // CSV: username,passwordHash,createdAt,lastSeen,isOnline
        file << escapeCSV(username) << ","
             << escapeCSV(passwordHash) << ","
             << now << ","
             << now << ","
             << "1\n";
        
        file.close();
        return true;
    }
    
    bool setUserOnline(const std::string& username, bool online) {
        std::lock_guard<std::mutex> lock(mtx);
        return updateUserStatus(username, online);
    }
    
    std::vector<std::string> getOnlineUsers() {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<std::string> onlineUsers;
        
        std::ifstream file(usersFile);
        if (!file.is_open()) return onlineUsers;
        
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            UserRecord user = parseUser(line);
            if (user.isOnline) {
                onlineUsers.push_back(user.username);
            }
        }
        
        return onlineUsers;
    }
    
    std::vector<UserRecord> getAllUsers() {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<UserRecord> users;
        
        std::ifstream file(usersFile);
        if (!file.is_open()) return users;
        
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            users.push_back(parseUser(line));
        }
        
        return users;
    }
    
    // ============ Groups ============
    
    bool saveGroup(const std::string& groupName, const std::string& createdBy) {
        std::lock_guard<std::mutex> lock(mtx);
        
        if (groupExists(groupName)) return false;
        
        std::ofstream file(groupsFile, std::ios::app);
        if (!file.is_open()) return false;
        
        uint64_t now = time(nullptr);
        
        // CSV: groupName,createdBy,createdAt,members
        file << escapeCSV(groupName) << ","
             << escapeCSV(createdBy) << ","
             << now << ","
             << escapeCSV(createdBy) << "\n";
        
        file.close();
        return true;
    }
    
    bool addGroupMember(const std::string& groupName, const std::string& username) {
        // Read all groups, modify, rewrite
        std::vector<GroupRecord> groups;
        
        std::ifstream inFile(groupsFile);
        if (!inFile.is_open()) return false;
        
        std::string line;
        std::getline(inFile, line); // Skip header
        
        bool found = false;
        while (std::getline(inFile, line)) {
            GroupRecord group = parseGroup(line);
            if (group.groupName == groupName) {
                // Check if already member
                bool isMember = false;
                for (const auto& m : group.members) {
                    if (m == username) {
                        isMember = true;
                        break;
                    }
                }
                if (!isMember) {
                    group.members.push_back(username);
                }
                found = true;
            }
            groups.push_back(group);
        }
        inFile.close();
        
        if (!found) return false;
        
        // Rewrite file
        std::ofstream outFile(groupsFile);
        outFile << "groupName,createdBy,createdAt,members\n";
        for (const auto& g : groups) {
            outFile << escapeCSV(g.groupName) << ","
                    << escapeCSV(g.createdBy) << ","
                    << g.createdAt << ","
                    << joinMembers(g.members) << "\n";
        }
        
        return true;
    }
    
    std::vector<std::string> getGroupMembers(const std::string& groupName) {
        std::lock_guard<std::mutex> lock(mtx);
        
        std::ifstream file(groupsFile);
        if (!file.is_open()) return {};
        
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            GroupRecord group = parseGroup(line);
            if (group.groupName == groupName) {
                return group.members;
            }
        }
        
        return {};
    }
    
    bool removeGroupMember(const std::string& groupName, const std::string& username) {
        // Read all groups, modify, rewrite
        std::vector<GroupRecord> groups;
        
        std::ifstream inFile(groupsFile);
        if (!inFile.is_open()) return false;
        
        std::string line;
        std::getline(inFile, line); // Skip header
        
        bool found = false;
        while (std::getline(inFile, line)) {
            GroupRecord group = parseGroup(line);
            if (group.groupName == groupName) {
                // Remove member
                auto it = std::find(group.members.begin(), group.members.end(), username);
                if (it != group.members.end()) {
                    group.members.erase(it);
                    found = true;
                }
            }
            groups.push_back(group);
        }
        inFile.close();
        
        if (!found) return false;
        
        // Rewrite file
        std::ofstream outFile(groupsFile);
        outFile << "groupName,createdBy,createdAt,members\n";
        for (const auto& g : groups) {
            outFile << escapeCSV(g.groupName) << ","
                    << escapeCSV(g.createdBy) << ","
                    << g.createdAt << ","
                    << joinMembers(g.members) << "\n";
        }
        
        return true;
    }
    
    bool isGroupMember(const std::string& groupName, const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        
        std::ifstream file(groupsFile);
        if (!file.is_open()) return false;
        
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            GroupRecord group = parseGroup(line);
            if (group.groupName == groupName) {
                return std::find(group.members.begin(), group.members.end(), username) != group.members.end();
            }
        }
        
        return false;
    }
    
    // Get all groups with info if user is member
    std::vector<std::pair<std::string, bool>> getAllGroupsWithMembership(const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<std::pair<std::string, bool>> result;
        
        std::ifstream file(groupsFile);
        if (!file.is_open()) return result;
        
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            GroupRecord group = parseGroup(line);
            bool isMember = std::find(group.members.begin(), group.members.end(), username) != group.members.end();
            result.push_back({group.groupName, isMember});
        }
        
        return result;
    }

private:
    void createDirectory(const std::string& dir) {
        #ifdef _WIN32
        CreateDirectoryA(dir.c_str(), nullptr);
        #else
        mkdir(dir.c_str(), 0755);
        #endif
    }
    
    void initializeFiles() {
        // Initialize messages file
        std::ifstream testMsg(messagesFile);
        if (!testMsg.good()) {
            std::ofstream file(messagesFile);
            file << "id,sender,recipient,content,timestamp,isGroup,isFile,filename\n";
            file.close();
        }
        
        // Initialize users file
        std::ifstream testUsr(usersFile);
        if (!testUsr.good()) {
            std::ofstream file(usersFile);
            file << "username,passwordHash,createdAt,lastSeen,isOnline\n";
            file.close();
        }
        
        // Initialize groups file
        std::ifstream testGrp(groupsFile);
        if (!testGrp.good()) {
            std::ofstream file(groupsFile);
            file << "groupName,createdBy,createdAt,members\n";
            file.close();
        }
    }
    
    void loadNextMessageId() {
        std::ifstream file(messagesFile);
        if (!file.is_open()) return;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == 'i') continue; // Skip header
            
            size_t pos = line.find(',');
            if (pos != std::string::npos) {
                uint32_t id = std::stoul(line.substr(0, pos));
                if (id >= nextMessageId) {
                    nextMessageId = id + 1;
                }
            }
        }
    }
    
    std::string escapeCSV(const std::string& str) {
        std::string result = str;
        // Replace commas and newlines
        for (char& c : result) {
            if (c == ',') c = ';';
            if (c == '\n') c = ' ';
            if (c == '\r') c = ' ';
        }
        return result;
    }
    
    std::vector<std::string> splitCSV(const std::string& line) {
        std::vector<std::string> result;
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, ',')) {
            result.push_back(item);
        }
        return result;
    }
    
    ChatMessage parseMessage(const std::string& line) {
        ChatMessage msg = {0};
        auto parts = splitCSV(line);
        if (parts.size() >= 8) {
            msg.id = std::stoul(parts[0]);
            msg.sender = parts[1];
            msg.recipient = parts[2];
            msg.content = parts[3];
            msg.timestamp = std::stoull(parts[4]);
            msg.isGroup = (parts[5] == "1");
            msg.isFile = (parts[6] == "1");
            msg.filename = parts[7];
        }
        return msg;
    }
    
    UserRecord parseUser(const std::string& line) {
        UserRecord user;
        auto parts = splitCSV(line);
        if (parts.size() >= 5) {
            user.username = parts[0];
            user.passwordHash = parts[1];
            user.createdAt = std::stoull(parts[2]);
            user.lastSeen = std::stoull(parts[3]);
            user.isOnline = (parts[4] == "1");
        }
        return user;
    }
    
    GroupRecord parseGroup(const std::string& line) {
        GroupRecord group;
        auto parts = splitCSV(line);
        if (parts.size() >= 4) {
            group.groupName = parts[0];
            group.createdBy = parts[1];
            group.createdAt = std::stoull(parts[2]);
            // Members are semicolon-separated
            std::stringstream ss(parts[3]);
            std::string member;
            while (std::getline(ss, member, ';')) {
                if (!member.empty()) {
                    group.members.push_back(member);
                }
            }
        }
        return group;
    }
    
    std::string joinMembers(const std::vector<std::string>& members) {
        std::string result;
        for (size_t i = 0; i < members.size(); i++) {
            if (i > 0) result += ";";
            result += members[i];
        }
        return result;
    }
    
    bool userExists(const std::string& username) {
        std::ifstream file(usersFile);
        if (!file.is_open()) return false;
        
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            UserRecord user = parseUser(line);
            if (user.username == username) {
                return true;
            }
        }
        return false;
    }
    
    bool groupExists(const std::string& groupName) {
        std::ifstream file(groupsFile);
        if (!file.is_open()) return false;
        
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            GroupRecord group = parseGroup(line);
            if (group.groupName == groupName) {
                return true;
            }
        }
        return false;
    }
    
    bool updateLastSeen(const std::string& username) {
        return updateUserStatus(username, true);
    }
    
    bool updateUserStatus(const std::string& username, bool online) {
        std::vector<UserRecord> users;
        
        std::ifstream inFile(usersFile);
        if (!inFile.is_open()) return false;
        
        std::string line;
        std::getline(inFile, line); // Skip header
        
        while (std::getline(inFile, line)) {
            UserRecord user = parseUser(line);
            if (user.username == username) {
                user.isOnline = online;
                user.lastSeen = time(nullptr);
            }
            users.push_back(user);
        }
        inFile.close();
        
        // Rewrite file
        std::ofstream outFile(usersFile);
        outFile << "username,passwordHash,createdAt,lastSeen,isOnline\n";
        for (const auto& u : users) {
            outFile << escapeCSV(u.username) << ","
                    << escapeCSV(u.passwordHash) << ","
                    << u.createdAt << ","
                    << u.lastSeen << ","
                    << (u.isOnline ? "1" : "0") << "\n";
        }
        
        return true;
    }
};

#endif // DATABASE_MANAGER_H
