#ifndef TOPIC_MANAGER_H
#define TOPIC_MANAGER_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include <mutex>

class TopicManager {
private:
    std::map<std::string, std::set<std::string>> topics; // topic -> set of subscribers
    std::mutex mtx;

public:
    TopicManager() = default;
    ~TopicManager() = default;

    // Subscribe user to topic
    bool subscribe(const std::string& topic, const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        topics[topic].insert(username);
        return true;
    }

    // Unsubscribe user from topic
    bool unsubscribe(const std::string& topic, const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = topics.find(topic);
        if (it != topics.end()) {
            it->second.erase(username);
            if (it->second.empty()) {
                topics.erase(it);
            }
            return true;
        }
        return false;
    }

    // Remove user from all topics
    void removeUserFromAllTopics(const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        
        for (auto& topicPair : topics) {
            topicPair.second.erase(username);
        }
        
        // Clean up empty topics
        for (auto it = topics.begin(); it != topics.end(); ) {
            if (it->second.empty()) {
                it = topics.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Get subscribers of a topic
    std::set<std::string> getSubscribers(const std::string& topic) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = topics.find(topic);
        if (it != topics.end()) {
            return it->second;
        }
        return std::set<std::string>();
    }

    // Check if user is subscribed to topic
    bool isSubscribed(const std::string& topic, const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = topics.find(topic);
        if (it != topics.end()) {
            return it->second.find(username) != it->second.end();
        }
        return false;
    }

    // Get all topics user is subscribed to
    std::vector<std::string> getUserTopics(const std::string& username) {
        std::lock_guard<std::mutex> lock(mtx);
        
        std::vector<std::string> userTopics;
        for (const auto& topicPair : topics) {
            if (topicPair.second.find(username) != topicPair.second.end()) {
                userTopics.push_back(topicPair.first);
            }
        }
        return userTopics;
    }

    // Get topic count
    size_t getTopicCount() const {
        return topics.size();
    }

    // Get all topics
    std::vector<std::string> getAllTopics() {
        std::lock_guard<std::mutex> lock(mtx);
        
        std::vector<std::string> topicList;
        for (const auto& topicPair : topics) {
            topicList.push_back(topicPair.first);
        }
        return topicList;
    }
};

#endif // TOPIC_MANAGER_H
