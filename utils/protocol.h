#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <cstring>

// Default ports and buffer sizes
#define DEFAULT_PORT 8080
#define MAX_BUFFER_SIZE 4096
#define MAX_TOPIC_LEN 32
#define MAX_USERNAME_LEN 32
#define FILE_CHUNK_SIZE 8192

// =======================
// Message types (low-level)
// =======================
enum MessageType {
    MSG_LOGIN = 1,
    MSG_LOGOUT,

    MSG_SUBSCRIBE,
    MSG_UNSUBSCRIBE,

    MSG_PUBLISH_TEXT,
    MSG_PUBLISH_FILE,
    MSG_FILE_DATA,

    MSG_ERROR,
    MSG_ACK,
    
    // Online status messages
    MSG_USER_ONLINE,
    MSG_USER_OFFLINE,
    MSG_USER_LIST,
    MSG_REQUEST_USER_LIST,
    
    // History messages
    MSG_REQUEST_HISTORY,
    MSG_HISTORY_DATA,
    
    // Group messages
    MSG_GROUP_CREATED,
    MSG_GROUP_LIST,
    
    // Game messages
    MSG_GAME = 50
};

// =======================
// Packet types (protocol-level)
// =======================
typedef enum {
    LTM_LOGIN = 1,
    LTM_JOIN_GRP,
    LTM_LEAVE_GRP,
    LTM_MESSAGE,
    LTM_HISTORY,

    LTM_FILE_META,
    LTM_FILE_CHUNK,
    LTM_DOWNLOAD,

    LTM_ERROR,

    LTM_AUTH_REQ,
    LTM_AUTH_RESP
} PacketType;

#pragma pack(push, 1)
struct PacketHeader {
    uint32_t msgType;        // MessageType or PacketType
    uint32_t payloadLength; // Length of the payload
    uint32_t messageId;     // Unique message ID
    uint64_t timestamp;     // Timestamp
    uint8_t version;        // Protocol version
    uint8_t flags;          // Bit flags
    char sender[MAX_USERNAME_LEN];
    char topic[MAX_TOPIC_LEN];
    uint32_t checksum;      // CRC32
};
#pragma pack(pop)

#endif // PROTOCOL_H
