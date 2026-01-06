#ifndef FILE_TRANSFER_MANAGER_H
#define FILE_TRANSFER_MANAGER_H

#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

struct FileTransfer {
    std::string filename;
    uint32_t fileSize;
    uint32_t receivedSize;
    std::vector<char> data;
    std::string sender;
    std::string recipient; // Can be username or topic/group name
    bool isComplete;
    
    FileTransfer() : fileSize(0), receivedSize(0), isComplete(false) {}
};

class FileTransferManager {
private:
    std::map<uint32_t, FileTransfer> activeTransfers; // messageId -> FileTransfer
    std::mutex mtx;

public:
    FileTransferManager() = default;
    ~FileTransferManager() = default;

    // Start a new file transfer
    bool startTransfer(uint32_t messageId, const std::string& filename, 
                       uint32_t fileSize, const std::string& sender, 
                       const std::string& recipient) {
        std::lock_guard<std::mutex> lock(mtx);
        
        FileTransfer ft;
        ft.filename = filename;
        ft.fileSize = fileSize;
        ft.receivedSize = 0;
        ft.sender = sender;
        ft.recipient = recipient;
        ft.isComplete = false;
        
        activeTransfers[messageId] = ft;
        return true;
    }

    // Add data chunk to transfer
    bool addChunk(uint32_t messageId, const std::vector<char>& chunk) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = activeTransfers.find(messageId);
        if (it == activeTransfers.end()) {
            return false;
        }
        
        FileTransfer& ft = it->second;
        ft.data.insert(ft.data.end(), chunk.begin(), chunk.end());
        ft.receivedSize += chunk.size();
        
        if (ft.receivedSize >= ft.fileSize) {
            ft.isComplete = true;
        }
        
        return true;
    }

    // Get transfer info
    FileTransfer* getTransfer(uint32_t messageId) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = activeTransfers.find(messageId);
        if (it != activeTransfers.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    // Check if transfer exists
    bool exists(uint32_t messageId) {
        std::lock_guard<std::mutex> lock(mtx);
        return activeTransfers.find(messageId) != activeTransfers.end();
    }

    // Check if transfer is complete
    bool isComplete(uint32_t messageId) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = activeTransfers.find(messageId);
        if (it != activeTransfers.end()) {
            return it->second.isComplete;
        }
        return false;
    }

    // Get transfer progress (0.0 - 1.0)
    float getProgress(uint32_t messageId) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = activeTransfers.find(messageId);
        if (it != activeTransfers.end() && it->second.fileSize > 0) {
            return (float)it->second.receivedSize / it->second.fileSize;
        }
        return 0.0f;
    }

    // Remove completed transfer
    bool removeTransfer(uint32_t messageId) {
        std::lock_guard<std::mutex> lock(mtx);
        return activeTransfers.erase(messageId) > 0;
    }

    // Get sender of transfer
    std::string getSender(uint32_t messageId) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = activeTransfers.find(messageId);
        if (it != activeTransfers.end()) {
            return it->second.sender;
        }
        return "";
    }

    // Get recipient of transfer
    std::string getRecipient(uint32_t messageId) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = activeTransfers.find(messageId);
        if (it != activeTransfers.end()) {
            return it->second.recipient;
        }
        return "";
    }

    // Get active transfer count
    size_t getActiveCount() const {
        return activeTransfers.size();
    }
};

#endif // FILE_TRANSFER_MANAGER_H
