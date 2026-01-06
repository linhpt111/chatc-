#include "chat_client.h"
#include "../utils/caro_game.h"
#include <gtk/gtk.h>
#include <set>
#include <map>
#include <vector>
#include <utility>
#include <shellapi.h>

// Global client instance
ChatClient* g_client = nullptr;
std::string g_currentRecipient = "";  // Current selected user/group for chat
bool g_isGroupChat = false;           // true if chatting to group, false if DM
std::set<std::string> g_joinedGroups; // Groups that current user has joined
std::map<std::string, std::string> g_chatHistory; // Cache chat history for each conversation
std::vector<std::string> g_downloadedFiles; // List of downloaded files for click handling
std::string g_lastReceivedFile = ""; // Last received file path for quick open

// Caro game state
CaroState g_caroState;

// GTK UI structures
typedef struct {
    GtkWidget* window;
    GtkWidget* loginBox;
    GtkWidget* chatBox;
    GtkWidget* usernameEntry;
    GtkWidget* serverEntry;
    GtkWidget* portEntry;
    GtkWidget* connectBtn;
    GtkWidget* chatView;
    GtkTextBuffer* chatBuffer;
    GtkWidget* messageEntry;
    GtkWidget* groupEntry;
    GtkWidget* sendBtn;
    GtkWidget* sendFileBtn;
    GtkWidget* createGroupBtn;
    GtkWidget* joinGroupBtn;
    GtkWidget* leaveGroupBtn;
    GtkWidget* statusLabel;
    GtkWidget* chatTitleLabel;
    GtkWidget* onlineUsersView;
    GtkListStore* onlineUsersStore;
    GtkWidget* groupsView;
    GtkListStore* groupsStore;
    // Game widgets
    GtkWidget* playCaroBtn;
    GtkWidget* caroSizeCombo;
} AppWidgets;

AppWidgets* app = nullptr;

// Forward declarations
void update_chat_title();

// Helper functions for GTK thread-safe updates
gboolean update_online_users_list(gpointer data) {
    std::vector<std::string>* users = static_cast<std::vector<std::string>*>(data);
    
    gtk_list_store_clear(app->onlineUsersStore);
    
    for (const auto& user : *users) {
        GtkTreeIter iter;
        gtk_list_store_append(app->onlineUsersStore, &iter);
        gtk_list_store_set(app->onlineUsersStore, &iter, 
                          0, "●",
                          1, user.c_str(), 
                          -1);
    }
    
    delete users;
    return G_SOURCE_REMOVE;
}

gboolean add_online_user_ui(gpointer data) {
    std::string* username = static_cast<std::string*>(data);
    
    GtkTreeIter treeIter;
    gtk_list_store_append(app->onlineUsersStore, &treeIter);
    gtk_list_store_set(app->onlineUsersStore, &treeIter, 
                      0, "●",
                      1, username->c_str(), 
                      -1);
    
    delete username;
    return G_SOURCE_REMOVE;
}

gboolean remove_online_user_ui(gpointer data) {
    std::string* username = static_cast<std::string*>(data);
    
    GtkTreeIter treeIter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(app->onlineUsersStore), &treeIter);
    
    while (valid) {
        gchar* name;
        gtk_tree_model_get(GTK_TREE_MODEL(app->onlineUsersStore), &treeIter, 1, &name, -1);
        
        if (name && *username == name) {
            g_free(name);
            gtk_list_store_remove(app->onlineUsersStore, &treeIter);
            break;
        }
        
        g_free(name);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(app->onlineUsersStore), &treeIter);
    }
    
    delete username;
    return G_SOURCE_REMOVE;
}

// Struct for passing message data to UI thread
struct MessageData {
    std::string sender;
    std::string topic;
    std::string message;
    std::string conversationKey;
};

// Thread-safe message display
gboolean display_message_ui(gpointer data) {
    MessageData* msgData = static_cast<MessageData*>(data);
    
    std::string display = msgData->sender + ": " + msgData->message + "\n";
    
    // If this message is for the current conversation, display it
    if (msgData->conversationKey == g_currentRecipient) {
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(app->chatBuffer, &iter);
        gtk_text_buffer_insert(app->chatBuffer, &iter, display.c_str(), -1);
    }
    
    // Also save to chat history cache
    g_chatHistory[msgData->conversationKey] += display;
    
    delete msgData;
    return G_SOURCE_REMOVE;
}

// Struct for passing file data to UI thread
struct FileData {
    std::string sender;
    std::string filename;
    std::string filepath;
    uint32_t size;
};

// Convert UTF-8 to wide string for Windows Unicode APIs
std::wstring utf8_to_wide(const std::string& utf8str) {
    if (utf8str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), -1, &result[0], size);
    return result;
}

// Callback when clicking Open button embedded in chat
void on_embedded_open_clicked(GtkButton* button, gpointer user_data) {
    (void)user_data; // unused
    const char* filepath = (const char*)g_object_get_data(G_OBJECT(button), "filepath");
    if (filepath && filepath[0] != '\0') {
        // Convert UTF-8 path to wide string for proper Unicode handling
        std::wstring widePath = utf8_to_wide(filepath);
        int result = (int)(intptr_t)ShellExecuteW(nullptr, L"open", widePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (result <= 32) {
            g_print("Failed to open file: %s (error: %d)\n", filepath, result);
        }
    }
}

// Convert wide string to UTF-8
std::string wide_to_utf8(const std::wstring& widestr) {
    if (widestr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, widestr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, widestr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

// Thread-safe file received display
gboolean display_file_ui(gpointer data) {
    FileData* fileData = static_cast<FileData*>(data);
    
    // Get absolute path for the file using Unicode APIs
    std::wstring widePath = utf8_to_wide(fileData->filepath);
    wchar_t absPath[MAX_PATH];
    DWORD len = GetFullPathNameW(widePath.c_str(), MAX_PATH, absPath, nullptr);
    std::string absolutePath = (len > 0) ? wide_to_utf8(absPath) : fileData->filepath;
    
    g_downloadedFiles.push_back(absolutePath);
    g_lastReceivedFile = absolutePath;
    
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(app->chatBuffer, &iter);
    
    // Display file received message
    std::string display = "[FILE] Received '" + fileData->filename + "' from " + fileData->sender + " ";
    gtk_text_buffer_insert(app->chatBuffer, &iter, display.c_str(), -1);
    
    // Create embedded Open button using child anchor
    gtk_text_buffer_get_end_iter(app->chatBuffer, &iter);
    GtkTextChildAnchor* anchor = gtk_text_buffer_create_child_anchor(app->chatBuffer, &iter);
    
    GtkWidget* openBtn = gtk_button_new_with_label("Open");
    gtk_widget_set_size_request(openBtn, 60, 24);
    
    // Store filepath in button using g_object_set_data_full (auto-frees on destroy)
    g_object_set_data_full(G_OBJECT(openBtn), "filepath", g_strdup(absolutePath.c_str()), g_free);
    g_signal_connect(openBtn, "clicked", G_CALLBACK(on_embedded_open_clicked), nullptr);
    
    gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(app->chatView), openBtn, anchor);
    gtk_widget_show(openBtn);
    
    // Add newline after button
    gtk_text_buffer_get_end_iter(app->chatBuffer, &iter);
    gtk_text_buffer_insert(app->chatBuffer, &iter, "\n", -1);
    
    // Save to chat history
    g_chatHistory[g_currentRecipient] += "[FILE] Received '" + fileData->filename + "' from " + fileData->sender + "\n";
    
    delete fileData;
    return G_SOURCE_REMOVE;
}

// Struct for history data
struct HistoryData {
    std::string sender;
    std::string topic;
    std::string message;
    time_t timestamp;
};

// Thread-safe history display
gboolean display_history_ui(gpointer data) {
    HistoryData* histData = static_cast<HistoryData*>(data);
    
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(app->chatBuffer, &iter);
    
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M", localtime(&histData->timestamp));
    
    std::string display = "[" + std::string(timeStr) + "] " + histData->sender + ": " + histData->message + "\n";
    gtk_text_buffer_insert(app->chatBuffer, &iter, display.c_str(), -1);
    
    delete histData;
    return G_SOURCE_REMOVE;
}

// Open file when clicking on file link
void open_file(const char* filepath) {
    if (filepath) {
        ShellExecuteA(nullptr, "open", filepath, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

// Handler for clicking on chat text (for file links)
gboolean on_chat_click(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
    if (event->type != GDK_BUTTON_PRESS || event->button != 1) {
        return FALSE;
    }
    
    GtkTextView* textView = GTK_TEXT_VIEW(widget);
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(textView);
    
    gint x, y;
    gtk_text_view_window_to_buffer_coords(textView, GTK_TEXT_WINDOW_WIDGET, 
                                          (gint)event->x, (gint)event->y, &x, &y);
    
    GtkTextIter iter;
    gtk_text_view_get_iter_at_location(textView, &iter, x, y);
    
    // Check if there's a tag with filepath at this location
    GSList* tags = gtk_text_iter_get_tags(&iter);
    for (GSList* tagp = tags; tagp != nullptr; tagp = tagp->next) {
        GtkTextTag* tag = GTK_TEXT_TAG(tagp->data);
        gchar* filepath = (gchar*)g_object_get_data(G_OBJECT(tag), "filepath");
        if (filepath) {
            open_file(filepath);
            g_slist_free(tags);
            return TRUE;
        }
    }
    g_slist_free(tags);
    
    return FALSE;
}

// Update chat title based on current selection
void update_chat_title() {
    std::string title;
    if (g_currentRecipient.empty()) {
        title = "Group chat  Broadcasting to everyone";
    } else if (g_isGroupChat) {
        title = "Group: " + g_currentRecipient;
    } else {
        title = "Chat with: " + g_currentRecipient;
    }
    gtk_label_set_text(GTK_LABEL(app->chatTitleLabel), title.c_str());
}

// Callback when user clicks on online user list
void on_user_selected(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeModel* model = gtk_tree_view_get_model(tree_view);
    
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gchar* username;
        gtk_tree_model_get(model, &iter, 1, &username, -1);
        
        if (username) {
            // Don't select yourself
            if (g_client && std::string(username) != g_client->getUsername()) {
                // Save current chat history before switching
                if (!g_currentRecipient.empty()) {
                    GtkTextIter start, end;
                    gtk_text_buffer_get_bounds(app->chatBuffer, &start, &end);
                    gchar* text = gtk_text_buffer_get_text(app->chatBuffer, &start, &end, FALSE);
                    g_chatHistory[g_currentRecipient] = text ? text : "";
                    g_free(text);
                }
                
                g_currentRecipient = username;
                g_isGroupChat = false;
                update_chat_title();
                
                // Restore chat history or request from server
                if (g_chatHistory.find(g_currentRecipient) != g_chatHistory.end()) {
                    gtk_text_buffer_set_text(app->chatBuffer, g_chatHistory[g_currentRecipient].c_str(), -1);
                } else {
                    gtk_text_buffer_set_text(app->chatBuffer, "", -1);
                    g_client->requestHistory(g_currentRecipient);
                }
            }
            g_free(username);
        }
    }
}

// Callback when user clicks on group list
void on_group_selected(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeModel* model = gtk_tree_view_get_model(tree_view);
    
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gchar* groupname;
        gtk_tree_model_get(model, &iter, 1, &groupname, -1);
        
        if (groupname) {
            std::string groupStr = groupname;
            
            // If not joined yet, join first
            if (g_joinedGroups.find(groupStr) == g_joinedGroups.end()) {
                if (g_client && g_client->joinGroup(groupStr)) {
                    g_joinedGroups.insert(groupStr);
                    // Update color to green
                    gtk_list_store_set(app->groupsStore, &iter, 2, "#00aa00", -1);
                }
            }
            
            // Save current chat history before switching
            if (!g_currentRecipient.empty()) {
                GtkTextIter start, end;
                gtk_text_buffer_get_bounds(app->chatBuffer, &start, &end);
                gchar* text = gtk_text_buffer_get_text(app->chatBuffer, &start, &end, FALSE);
                g_chatHistory[g_currentRecipient] = text ? text : "";
                g_free(text);
            }
            
            g_currentRecipient = groupStr;
            g_isGroupChat = true;
            update_chat_title();
            
            // Restore chat history or request from server
            if (g_chatHistory.find(g_currentRecipient) != g_chatHistory.end()) {
                gtk_text_buffer_set_text(app->chatBuffer, g_chatHistory[g_currentRecipient].c_str(), -1);
            } else {
                gtk_text_buffer_set_text(app->chatBuffer, "", -1);
                g_client->requestHistory(g_currentRecipient);
            }
            
            g_free(groupname);
        }
    }
}

// Add group to groups list (with color: green=joined, gray=not joined)
void add_group_to_list(const std::string& groupName, bool isJoined = false) {
    // Check if group already exists
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(app->groupsStore), &iter);
    
    while (valid) {
        gchar* name;
        gtk_tree_model_get(GTK_TREE_MODEL(app->groupsStore), &iter, 1, &name, -1);
        
        if (name && groupName == name) {
            g_free(name);
            // Update color if already exists
            const char* color = isJoined ? "#00aa00" : "#888888";
            gtk_list_store_set(app->groupsStore, &iter, 2, color, -1);
            return;
        }
        
        g_free(name);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(app->groupsStore), &iter);
    }
    
    // Add new group with color
    const char* color = isJoined ? "#00aa00" : "#888888";
    gtk_list_store_append(app->groupsStore, &iter);
    gtk_list_store_set(app->groupsStore, &iter, 
                      0, "#",
                      1, groupName.c_str(),
                      2, color,
                      -1);
}

// Update group color when joining/leaving
void update_group_color(const std::string& groupName, bool isJoined) {
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(app->groupsStore), &iter);
    
    while (valid) {
        gchar* name;
        gtk_tree_model_get(GTK_TREE_MODEL(app->groupsStore), &iter, 1, &name, -1);
        
        if (name && groupName == name) {
            const char* color = isJoined ? "#00aa00" : "#888888";
            gtk_list_store_set(app->groupsStore, &iter, 2, color, -1);
            
            if (isJoined) {
                g_joinedGroups.insert(groupName);
            } else {
                g_joinedGroups.erase(groupName);
            }
            
            g_free(name);
            return;
        }
        
        g_free(name);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(app->groupsStore), &iter);
    }
}

// Thread-safe wrapper for adding group from server broadcast
typedef struct {
    std::string groupName;
    std::string creator;
} GroupInfo;

gboolean add_group_ui(gpointer data) {
    GroupInfo* info = static_cast<GroupInfo*>(data);
    
    // Check if this user created/joined the group
    bool isJoined = (g_client && info->creator == g_client->getUsername());
    add_group_to_list(info->groupName, isJoined);
    
    if (isJoined) {
        g_joinedGroups.insert(info->groupName);
    }
    
    delete info;
    return G_SOURCE_REMOVE;
}

// Thread-safe wrapper for updating groups list from server
gboolean update_groups_list_ui(gpointer data) {
    auto* groups = static_cast<std::vector<std::pair<std::string, bool>>*>(data);
    
    gtk_list_store_clear(app->groupsStore);
    g_joinedGroups.clear();
    
    for (const auto& g : *groups) {
        add_group_to_list(g.first, g.second);
        if (g.second) {
            g_joinedGroups.insert(g.first);
        }
    }
    
    delete groups;
    return G_SOURCE_REMOVE;
}

// Function declarations
void build_ui();

// GTK callback functions
void on_connect_clicked(GtkButton* button, gpointer user_data) {
    const char* username = gtk_entry_get_text(GTK_ENTRY(app->usernameEntry));
    const char* server = gtk_entry_get_text(GTK_ENTRY(app->serverEntry));
    const char* port = gtk_entry_get_text(GTK_ENTRY(app->portEntry));
    
    if (strlen(username) == 0) {
        gtk_label_set_text(GTK_LABEL(app->statusLabel), "Please enter username");
        return;
    }
    
    g_client = new ChatClient();
    
    // Set callbacks - use g_idle_add for thread-safe GTK updates
    g_client->setMessageCallback([](const std::string& sender, const std::string& topic, const std::string& msg) {
        MessageData* data = new MessageData();
        data->sender = sender;
        data->topic = topic;
        data->message = msg;
        
        // Determine the conversation key (other user for DM, or group name)
        if (topic.find("dm_") == 0) {
            // It's a DM - the conversation key is the sender
            data->conversationKey = sender;
        } else {
            // It's a group message
            data->conversationKey = topic;
        }
        
        g_idle_add(display_message_ui, data);
    });
    
    g_client->setFileCallback([](const std::string& sender, const std::string& filename, uint32_t size) {
        FileData* data = new FileData();
        data->sender = sender;
        data->filename = filename;
        data->filepath = "downloads\\" + filename;
        data->size = size;
        
        g_idle_add(display_file_ui, data);
    });
    
    g_client->setUserStatusCallback([](const std::string& username, bool isOnline) {
        if (isOnline) {
            g_idle_add(add_online_user_ui, new std::string(username));
        } else {
            g_idle_add(remove_online_user_ui, new std::string(username));
        }
    });
    
    g_client->setUserListCallback([](const std::vector<std::string>& users) {
        g_idle_add(update_online_users_list, new std::vector<std::string>(users));
    });
    
    g_client->setHistoryCallback([](const std::string& sender, const std::string& topic, const std::string& msg, time_t ts) {
        HistoryData* data = new HistoryData();
        data->sender = sender;
        data->topic = topic;
        data->message = msg;
        data->timestamp = ts;
        
        g_idle_add(display_history_ui, data);
    });
    
    // Set callback for new group broadcast
    g_client->setGroupCallback([](const std::string& groupName, const std::string& creator) {
        GroupInfo* info = new GroupInfo();
        info->groupName = groupName;
        info->creator = creator;
        g_idle_add(add_group_ui, info);
    });
    
    // Set callback for groups list (received on login)
    g_client->setGroupListCallback([](const std::vector<std::pair<std::string, bool>>& groups) {
        g_idle_add(update_groups_list_ui, new std::vector<std::pair<std::string, bool>>(groups));
    });
    
    // Set callback for game messages
    g_client->setGameCallback([](const std::string& from, const std::string& payload) {
        // Process on main thread
        struct GameMsgData {
            std::string from;
            std::string payload;
        };
        auto* data = new GameMsgData{from, payload};
        
        g_idle_add([](gpointer user_data) -> gboolean {
            auto* d = static_cast<GameMsgData*>(user_data);
            
            // Set up sendGameMessage callback for CaroState
            g_caroState.sendGameMessage = [](const std::string& to, const std::string& msg) -> bool {
                if (g_client) {
                    return g_client->sendGameMessage(to, msg);
                }
                return false;
            };
            
            caro_handle_message(&g_caroState, d->from, d->payload);
            delete d;
            return G_SOURCE_REMOVE;
        }, data);
    });
    
    // Set up sendGameMessage for caro state
    g_caroState.sendGameMessage = [](const std::string& to, const std::string& msg) -> bool {
        if (g_client) {
            return g_client->sendGameMessage(to, msg);
        }
        return false;
    };
    
    if (g_client->connect(server, atoi(port), username)) {
        gtk_widget_hide(app->loginBox);
        gtk_widget_show(app->chatBox);
        
        std::string status = "Connected as: " + std::string(username);
        gtk_label_set_text(GTK_LABEL(app->statusLabel), status.c_str());
        gtk_window_set_title(GTK_WINDOW(app->window), ("CChatApp - " + std::string(username)).c_str());
        
        g_client->requestUserList();
    } else {
        gtk_label_set_text(GTK_LABEL(app->statusLabel), "Connection failed");
        delete g_client;
        g_client = nullptr;
    }
}

void on_send_clicked(GtkButton* button, gpointer user_data) {
    const char* message = gtk_entry_get_text(GTK_ENTRY(app->messageEntry));
    
    if (strlen(message) == 0) return;
    
    bool sent = false;
    
    if (!g_currentRecipient.empty()) {
        if (g_isGroupChat) {
            sent = g_client->sendGroupMessage(g_currentRecipient, message);
        } else {
            sent = g_client->sendDirectMessage(g_currentRecipient, message);
        }
    }
    
    if (sent) {
        // Show own message in chat
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(app->chatBuffer, &iter);
        std::string display = "You: " + std::string(message) + "\n";
        gtk_text_buffer_insert(app->chatBuffer, &iter, display.c_str(), -1);
        
        // Also save to chat history cache
        g_chatHistory[g_currentRecipient] += display;
        
        gtk_entry_set_text(GTK_ENTRY(app->messageEntry), "");
    }
}

void on_send_file_clicked(GtkButton* button, gpointer user_data) {
    if (g_currentRecipient.empty()) {
        return;
    }
    
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Choose File",
        GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        nullptr);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        bool sent = false;
        if (g_isGroupChat) {
            sent = g_client->sendFileToGroup(g_currentRecipient, filename);
        } else {
            sent = g_client->sendFileToUser(g_currentRecipient, filename);
        }
        
        if (sent) {
            // Show notification that file was sent
            GtkTextIter iter;
            gtk_text_buffer_get_end_iter(app->chatBuffer, &iter);
            
            // Extract just the filename from path
            std::string filepath(filename);
            size_t pos = filepath.find_last_of("\\/");
            std::string fname = (pos != std::string::npos) ? filepath.substr(pos + 1) : filepath;
            
            std::string display = "[FILE] You sent '" + fname + "'\n";
            gtk_text_buffer_insert(app->chatBuffer, &iter, display.c_str(), -1);
            
            // Save to chat history
            g_chatHistory[g_currentRecipient] += display;
        }
        
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

void on_create_group_clicked(GtkButton* button, gpointer user_data) {
    const char* group = gtk_entry_get_text(GTK_ENTRY(app->groupEntry));
    
    if (strlen(group) > 0 && g_client) {
        if (g_client->joinGroup(group)) {
            // Add group to list with joined=true color (broadcast will also add it but we add first for immediate feedback)
            g_joinedGroups.insert(group);
            add_group_to_list(group, true);
            
            g_currentRecipient = group;
            g_isGroupChat = true;
            update_chat_title();
            gtk_text_buffer_set_text(app->chatBuffer, "", -1);
            
            gtk_entry_set_text(GTK_ENTRY(app->groupEntry), "");
        }
    }
}

void on_join_group_clicked(GtkButton* button, gpointer user_data) {
    const char* group = gtk_entry_get_text(GTK_ENTRY(app->groupEntry));
    
    if (strlen(group) > 0 && g_client) {
        if (g_client->joinGroup(group)) {
            // Add group to list with joined=true color
            g_joinedGroups.insert(group);
            add_group_to_list(group, true);
            
            g_currentRecipient = group;
            g_isGroupChat = true;
            update_chat_title();
            gtk_text_buffer_set_text(app->chatBuffer, "", -1);
            g_client->requestHistory(g_currentRecipient);
            
            gtk_entry_set_text(GTK_ENTRY(app->groupEntry), "");
        }
    }
}

void on_leave_group_clicked(GtkButton* button, gpointer user_data) {
    if (!g_isGroupChat || g_currentRecipient.empty() || !g_client) {
        return;
    }
    
    // Leave the current group
    if (g_client->leaveGroup(g_currentRecipient)) {
        g_joinedGroups.erase(g_currentRecipient);
        update_group_color(g_currentRecipient, false);
        
        // Clear selection
        g_currentRecipient = "";
        g_isGroupChat = false;
        update_chat_title();
        gtk_text_buffer_set_text(app->chatBuffer, "", -1);
    }
}

// Play Caro button clicked
void on_play_caro_clicked(GtkButton* button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    // Must select a user (not group)
    if (g_currentRecipient.empty() || g_isGroupChat) {
        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   "Please select an online user to play with.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    // Can't play with yourself
    if (g_client && g_currentRecipient == g_client->getUsername()) {
        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   "You cannot play with yourself!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    // Already in game?
    if (g_caroState.inGame || g_caroState.waitingAccept) {
        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   "Please finish current game first.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    // Get board size from combo
    int boardSize = CARO_DEFAULT_SIZE;
    if (app->caroSizeCombo) {
        gchar* sizeText = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(app->caroSizeCombo));
        if (sizeText) {
            // Parse "10x10" -> 10
            boardSize = atoi(sizeText);
            g_free(sizeText);
        }
    }
    boardSize = caro_sanitize_size(boardSize);
    g_caroState.boardSize = boardSize;
    g_caroState.winLength = caro_required_in_a_row(boardSize);
    
    // Show rules dialog
    char rules[256];
    snprintf(rules, sizeof(rules),
             "Caro %dx%d\nWin with %d in a row.\n\nSend invite to %s?",
             boardSize, boardSize, g_caroState.winLength, g_currentRecipient.c_str());
    
    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_OK_CANCEL,
                                               "%s", rules);
    gint resp = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (resp != GTK_RESPONSE_OK) return;
    
    // Send invite
    g_caroState.opponent = g_currentRecipient;
    g_caroState.waitingAccept = true;
    g_caroState.mySymbol = 'X';
    g_caroState.oppSymbol = 'O';
    
    char payload[64];
    snprintf(payload, sizeof(payload), "INVITE|%d", boardSize);
    
    if (g_client) {
        g_client->sendGameMessage(g_currentRecipient, payload);
        
        // Show waiting message
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(app->chatBuffer, &iter);
        std::string display = "[GAME] Invite sent to " + g_currentRecipient + ". Waiting for response...\n";
        gtk_text_buffer_insert(app->chatBuffer, &iter, display.c_str(), -1);
    }
}

// Handle Enter key in message entry
gboolean on_message_key_press(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        on_send_clicked(nullptr, nullptr);
        return TRUE;
    }
    return FALSE;
}

void build_ui() {
    app = g_new0(AppWidgets, 1);
    
    // Main window
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "CChatApp");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 800, 600);
    gtk_container_set_border_width(GTK_CONTAINER(app->window), 0);
    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->window), mainBox);
    
    // Status label (hidden)
    app->statusLabel = gtk_label_new("Not connected");
    
    // ==================== LOGIN BOX ====================
    app->loginBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(app->loginBox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(app->loginBox, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(mainBox), app->loginBox, TRUE, TRUE, 0);
    
    GtkWidget* loginFrame = gtk_frame_new("Login");
    gtk_box_pack_start(GTK_BOX(app->loginBox), loginFrame, FALSE, FALSE, 0);
    
    GtkWidget* loginGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(loginGrid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(loginGrid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(loginGrid), 20);
    gtk_container_add(GTK_CONTAINER(loginFrame), loginGrid);
    
    gtk_grid_attach(GTK_GRID(loginGrid), gtk_label_new("Username:"), 0, 0, 1, 1);
    app->usernameEntry = gtk_entry_new();
    gtk_widget_set_size_request(app->usernameEntry, 200, -1);
    gtk_grid_attach(GTK_GRID(loginGrid), app->usernameEntry, 1, 0, 1, 1);
    
    gtk_grid_attach(GTK_GRID(loginGrid), gtk_label_new("Server:"), 0, 1, 1, 1);
    app->serverEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->serverEntry), "127.0.0.1");
    gtk_grid_attach(GTK_GRID(loginGrid), app->serverEntry, 1, 1, 1, 1);
    
    gtk_grid_attach(GTK_GRID(loginGrid), gtk_label_new("Port:"), 0, 2, 1, 1);
    app->portEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->portEntry), "8080");
    gtk_grid_attach(GTK_GRID(loginGrid), app->portEntry, 1, 2, 1, 1);
    
    app->connectBtn = gtk_button_new_with_label("Connect");
    g_signal_connect(app->connectBtn, "clicked", G_CALLBACK(on_connect_clicked), nullptr);
    gtk_grid_attach(GTK_GRID(loginGrid), app->connectBtn, 0, 3, 2, 1);
    
    // ==================== CHAT BOX ====================
    app->chatBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), app->chatBox, TRUE, TRUE, 0);
    
    // ========== LEFT PANEL: Online Users + Groups ==========
    GtkWidget* leftPanel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(leftPanel, 200, -1);
    gtk_box_pack_start(GTK_BOX(app->chatBox), leftPanel, FALSE, FALSE, 0);
    
    // Add left border
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, 
        "box { border-right: 1px solid #ccc; background: #f5f5f5; }", -1, nullptr);
    gtk_style_context_add_provider(gtk_widget_get_style_context(leftPanel),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);
    
    // Online Users section
    GtkWidget* usersLabel = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(usersLabel), "<span color='#0066cc'><b>Online users</b></span>");
    gtk_widget_set_halign(usersLabel, GTK_ALIGN_START);
    gtk_widget_set_margin_start(usersLabel, 10);
    gtk_widget_set_margin_top(usersLabel, 10);
    gtk_box_pack_start(GTK_BOX(leftPanel), usersLabel, FALSE, FALSE, 5);
    
    // Online users list
    app->onlineUsersStore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    app->onlineUsersView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->onlineUsersStore));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(app->onlineUsersView), FALSE);
    g_signal_connect(app->onlineUsersView, "row-activated", G_CALLBACK(on_user_selected), nullptr);
    
    GtkCellRenderer* iconRenderer = gtk_cell_renderer_text_new();
    g_object_set(iconRenderer, "foreground", "green", NULL);
    GtkTreeViewColumn* iconCol = gtk_tree_view_column_new_with_attributes("", iconRenderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(app->onlineUsersView), iconCol);
    
    GtkCellRenderer* nameRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* nameCol = gtk_tree_view_column_new_with_attributes("", nameRenderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(app->onlineUsersView), nameCol);
    
    GtkWidget* usersScrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(usersScrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(usersScrolled), app->onlineUsersView);
    gtk_box_pack_start(GTK_BOX(leftPanel), usersScrolled, TRUE, TRUE, 0);
    
    // Groups section
    GtkWidget* groupsLabel = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(groupsLabel), "<b>Groups</b>");
    gtk_widget_set_halign(groupsLabel, GTK_ALIGN_START);
    gtk_widget_set_margin_start(groupsLabel, 10);
    gtk_widget_set_margin_top(groupsLabel, 10);
    gtk_box_pack_start(GTK_BOX(leftPanel), groupsLabel, FALSE, FALSE, 5);
    
    // Groups list - 3 columns: icon, name, color
    app->groupsStore = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    app->groupsView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->groupsStore));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(app->groupsView), FALSE);
    g_signal_connect(app->groupsView, "row-activated", G_CALLBACK(on_group_selected), nullptr);
    
    // Icon column with dynamic color from column 2
    GtkCellRenderer* groupIconRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* groupIconCol = gtk_tree_view_column_new_with_attributes("", groupIconRenderer, "text", 0, "foreground", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(app->groupsView), groupIconCol);
    
    // Name column with dynamic color from column 2
    GtkCellRenderer* groupNameRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* groupNameCol = gtk_tree_view_column_new_with_attributes("", groupNameRenderer, "text", 1, "foreground", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(app->groupsView), groupNameCol);
    
    GtkWidget* groupsScrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(groupsScrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(groupsScrolled), app->groupsView);
    gtk_box_pack_start(GTK_BOX(leftPanel), groupsScrolled, TRUE, TRUE, 0);
    
    // New group controls
    GtkWidget* newGroupBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(newGroupBox, 5);
    gtk_widget_set_margin_end(newGroupBox, 5);
    gtk_widget_set_margin_bottom(newGroupBox, 10);
    gtk_box_pack_start(GTK_BOX(leftPanel), newGroupBox, FALSE, FALSE, 5);
    
    app->groupEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->groupEntry), "New group");
    gtk_box_pack_start(GTK_BOX(newGroupBox), app->groupEntry, TRUE, TRUE, 0);
    
    app->createGroupBtn = gtk_button_new_with_label("Create");
    g_signal_connect(app->createGroupBtn, "clicked", G_CALLBACK(on_create_group_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(newGroupBox), app->createGroupBtn, FALSE, FALSE, 0);
    
    app->joinGroupBtn = gtk_button_new_with_label("Join");
    g_signal_connect(app->joinGroupBtn, "clicked", G_CALLBACK(on_join_group_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(newGroupBox), app->joinGroupBtn, FALSE, FALSE, 0);
    
    // Leave group button
    app->leaveGroupBtn = gtk_button_new_with_label("Leave");
    g_signal_connect(app->leaveGroupBtn, "clicked", G_CALLBACK(on_leave_group_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(newGroupBox), app->leaveGroupBtn, FALSE, FALSE, 0);
    
    // ========== RIGHT PANEL: Chat Area ==========
    GtkWidget* rightPanel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(app->chatBox), rightPanel, TRUE, TRUE, 0);
    
    // Chat title header
    app->chatTitleLabel = gtk_label_new("Group chat  Broadcasting to everyone");
    gtk_widget_set_halign(app->chatTitleLabel, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->chatTitleLabel, 10);
    gtk_widget_set_margin_top(app->chatTitleLabel, 10);
    gtk_widget_set_margin_bottom(app->chatTitleLabel, 5);
    
    GtkCssProvider* headerCss = gtk_css_provider_new();
    gtk_css_provider_load_from_data(headerCss, 
        "label { color: #0066cc; font-weight: bold; }", -1, nullptr);
    gtk_style_context_add_provider(gtk_widget_get_style_context(app->chatTitleLabel),
        GTK_STYLE_PROVIDER(headerCss), GTK_STYLE_PROVIDER_PRIORITY_USER);
    
    gtk_box_pack_start(GTK_BOX(rightPanel), app->chatTitleLabel, FALSE, FALSE, 0);
    
    // Chat view
    GtkWidget* chatScrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_hexpand(chatScrolled, TRUE);
    gtk_widget_set_vexpand(chatScrolled, TRUE);
    gtk_widget_set_margin_start(chatScrolled, 5);
    gtk_widget_set_margin_end(chatScrolled, 5);
    
    app->chatView = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->chatView), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->chatView), GTK_WRAP_WORD);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app->chatView), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(app->chatView), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app->chatView), 5);
    app->chatBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->chatView));
    
    // Connect click handler for file links
    g_signal_connect(app->chatView, "button-press-event", G_CALLBACK(on_chat_click), nullptr);
    
    gtk_container_add(GTK_CONTAINER(chatScrolled), app->chatView);
    gtk_box_pack_start(GTK_BOX(rightPanel), chatScrolled, TRUE, TRUE, 0);
    
    // Message input area
    GtkWidget* inputBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(inputBox, 10);
    gtk_widget_set_margin_end(inputBox, 10);
    gtk_widget_set_margin_top(inputBox, 5);
    gtk_widget_set_margin_bottom(inputBox, 10);
    gtk_box_pack_start(GTK_BOX(rightPanel), inputBox, FALSE, FALSE, 0);
    
    app->messageEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->messageEntry), "Type a message...");
    gtk_widget_set_hexpand(app->messageEntry, TRUE);
    g_signal_connect(app->messageEntry, "key-press-event", G_CALLBACK(on_message_key_press), nullptr);
    gtk_box_pack_start(GTK_BOX(inputBox), app->messageEntry, TRUE, TRUE, 0);
    
    app->sendFileBtn = gtk_button_new_with_label("Send file");
    g_signal_connect(app->sendFileBtn, "clicked", G_CALLBACK(on_send_file_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(inputBox), app->sendFileBtn, FALSE, FALSE, 0);
    
    app->sendBtn = gtk_button_new_with_label("Send");
    g_signal_connect(app->sendBtn, "clicked", G_CALLBACK(on_send_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(inputBox), app->sendBtn, FALSE, FALSE, 0);
    
    // Play Caro button
    app->playCaroBtn = gtk_button_new_with_label("Play Caro");
    g_signal_connect(app->playCaroBtn, "clicked", G_CALLBACK(on_play_caro_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(inputBox), app->playCaroBtn, FALSE, FALSE, 0);
    
    // Board size combo
    app->caroSizeCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->caroSizeCombo), "3x3");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->caroSizeCombo), "5x5");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->caroSizeCombo), "10x10");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->caroSizeCombo), 2);  // Default 10x10
    gtk_box_pack_start(GTK_BOX(inputBox), app->caroSizeCombo, FALSE, FALSE, 0);
    
    // Show window, hide chat initially
    gtk_widget_show_all(app->window);
    gtk_widget_hide(app->chatBox);
}

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);
    
    build_ui();
    
    gtk_main();
    
    if (g_client) {
        delete g_client;
    }
    
    return 0;
}
