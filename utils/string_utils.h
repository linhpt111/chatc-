#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>
#include <algorithm>

namespace StringUtils {

// Extract recipient from DM topic
// Topic format: "dm_user1_user2"
inline std::string extractRecipient(const std::string& topic, const std::string& sender) {
    if (topic.length() < 4 || topic.substr(0, 3) != "dm_") {
        return "";
    }
    
    size_t firstUnderscore = 3;
    size_t secondUnderscore = topic.find('_', firstUnderscore);
    
    if (secondUnderscore == std::string::npos) {
        return "";
    }
    
    std::string user1 = topic.substr(firstUnderscore, secondUnderscore - firstUnderscore);
    std::string user2 = topic.substr(secondUnderscore + 1);
    
    return (user1 == sender) ? user2 : user1;
}

// Create DM topic from two usernames (alphabetically ordered)
inline std::string createDMTopic(const std::string& user1, const std::string& user2) {
    if (user1 < user2) {
        return "dm_" + user1 + "_" + user2;
    } else {
        return "dm_" + user2 + "_" + user1;
    }
}

// Check if topic is a direct message topic
inline bool isDMTopic(const std::string& topic) {
    return topic.length() > 3 && topic.substr(0, 3) == "dm_";
}

// Trim whitespace from string
inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// Convert to lowercase
inline std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

} // namespace StringUtils

#endif // STRING_UTILS_H
