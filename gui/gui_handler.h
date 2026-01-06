#ifndef GUI_HANDLER_H
#define GUI_HANDLER_H

#include <gtk/gtk.h>
#include <string>
#include "../socket_client/chat_client.h"

// Global widgets
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
    GtkWidget* recipientEntry;
    GtkWidget* groupEntry;
    GtkWidget* sendBtn;
    GtkWidget* sendFileBtn;
    GtkWidget* joinGroupBtn;
    GtkWidget* leaveGroupBtn;
    GtkWidget* disconnectBtn;
    GtkWidget* statusLabel;
    GtkWidget* onlineUsersView;
    GtkListStore* onlineUsersStore;
} AppWidgets;

class GUIHandler {
private:
    AppWidgets widgets;
    ChatClient* client;
    GtkBuilder* builder;

public:
    GUIHandler() : client(nullptr), builder(nullptr) {}
    
    ~GUIHandler() {
        if (client) {
            delete client;
        }
        if (builder) {
            g_object_unref(builder);
        }
    }
    
    bool loadFromGlade(const char* filename) {
        builder = gtk_builder_new();
        GError* error = nullptr;
        
        if (!gtk_builder_add_from_file(builder, filename, &error)) {
            g_printerr("Error loading glade file: %s\n", error->message);
            g_error_free(error);
            return false;
        }
        
        // Get widgets from builder
        widgets.window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
        widgets.loginBox = GTK_WIDGET(gtk_builder_get_object(builder, "login_box"));
        widgets.chatBox = GTK_WIDGET(gtk_builder_get_object(builder, "chat_box"));
        widgets.usernameEntry = GTK_WIDGET(gtk_builder_get_object(builder, "username_entry"));
        widgets.serverEntry = GTK_WIDGET(gtk_builder_get_object(builder, "server_entry"));
        widgets.portEntry = GTK_WIDGET(gtk_builder_get_object(builder, "port_entry"));
        widgets.connectBtn = GTK_WIDGET(gtk_builder_get_object(builder, "connect_btn"));
        widgets.chatView = GTK_WIDGET(gtk_builder_get_object(builder, "chat_view"));
        widgets.messageEntry = GTK_WIDGET(gtk_builder_get_object(builder, "message_entry"));
        widgets.recipientEntry = GTK_WIDGET(gtk_builder_get_object(builder, "recipient_entry"));
        widgets.groupEntry = GTK_WIDGET(gtk_builder_get_object(builder, "group_entry"));
        widgets.sendBtn = GTK_WIDGET(gtk_builder_get_object(builder, "send_btn"));
        widgets.sendFileBtn = GTK_WIDGET(gtk_builder_get_object(builder, "send_file_btn"));
        widgets.joinGroupBtn = GTK_WIDGET(gtk_builder_get_object(builder, "join_group_btn"));
        widgets.leaveGroupBtn = GTK_WIDGET(gtk_builder_get_object(builder, "leave_group_btn"));
        widgets.disconnectBtn = GTK_WIDGET(gtk_builder_get_object(builder, "disconnect_btn"));
        widgets.statusLabel = GTK_WIDGET(gtk_builder_get_object(builder, "status_label"));
        
        widgets.chatBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widgets.chatView));
        
        return true;
    }
    
    void buildUI() {
        // Create window manually (without Glade)
        widgets.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(widgets.window), "Chat App - Pub/Sub");
        gtk_window_set_default_size(GTK_WINDOW(widgets.window), 600, 500);
        gtk_container_set_border_width(GTK_CONTAINER(widgets.window), 10);
        
        GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_add(GTK_CONTAINER(widgets.window), mainBox);
        
        // Status label
        widgets.statusLabel = gtk_label_new("Not connected");
        gtk_box_pack_start(GTK_BOX(mainBox), widgets.statusLabel, FALSE, FALSE, 0);
        
        // Create login and chat boxes
        createLoginBox(mainBox);
        createChatBox(mainBox);
        
        // Initially show login, hide chat
        gtk_widget_show_all(widgets.window);
        gtk_widget_hide(widgets.chatBox);
    }
    
    void show() {
        gtk_widget_show_all(widgets.window);
        if (widgets.chatBox) {
            gtk_widget_hide(widgets.chatBox);
        }
    }
    
    GtkWidget* getWindow() { return widgets.window; }
    AppWidgets* getWidgets() { return &widgets; }
    ChatClient* getClient() { return client; }
    
    void setClient(ChatClient* c) { client = c; }
    
    void appendMessage(const std::string& message) {
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(widgets.chatBuffer, &iter);
        gtk_text_buffer_insert(widgets.chatBuffer, &iter, message.c_str(), -1);
        gtk_text_buffer_insert(widgets.chatBuffer, &iter, "\n", -1);
    }
    
    void setStatus(const std::string& status) {
        gtk_label_set_text(GTK_LABEL(widgets.statusLabel), status.c_str());
    }
    
    void updateOnlineUsers(const std::vector<std::string>& users) {
        if (!widgets.onlineUsersStore) return;
        
        gtk_list_store_clear(widgets.onlineUsersStore);
        
        for (const auto& user : users) {
            GtkTreeIter iter;
            gtk_list_store_append(widgets.onlineUsersStore, &iter);
            gtk_list_store_set(widgets.onlineUsersStore, &iter, 
                              0, "●",  // Online indicator
                              1, user.c_str(), 
                              -1);
        }
    }
    
    void addOnlineUser(const std::string& username) {
        if (!widgets.onlineUsersStore) return;
        
        GtkTreeIter iter;
        gtk_list_store_append(widgets.onlineUsersStore, &iter);
        gtk_list_store_set(widgets.onlineUsersStore, &iter, 
                          0, "●",  // Online indicator
                          1, username.c_str(), 
                          -1);
    }
    
    void removeOnlineUser(const std::string& username) {
        if (!widgets.onlineUsersStore) return;
        
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(widgets.onlineUsersStore), &iter);
        
        while (valid) {
            gchar* name;
            gtk_tree_model_get(GTK_TREE_MODEL(widgets.onlineUsersStore), &iter, 1, &name, -1);
            
            if (name && username == name) {
                g_free(name);
                gtk_list_store_remove(widgets.onlineUsersStore, &iter);
                break;
            }
            
            g_free(name);
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(widgets.onlineUsersStore), &iter);
        }
    }
    
    void showChatView() {
        gtk_widget_hide(widgets.loginBox);
        gtk_widget_show(widgets.chatBox);
    }
    
    void showLoginView() {
        gtk_widget_show(widgets.loginBox);
        gtk_widget_hide(widgets.chatBox);
    }

private:
    void createLoginBox(GtkWidget* parent) {
        widgets.loginBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_box_pack_start(GTK_BOX(parent), widgets.loginBox, TRUE, TRUE, 0);
        
        GtkWidget* grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
        gtk_box_pack_start(GTK_BOX(widgets.loginBox), grid, FALSE, FALSE, 0);
        
        // Username
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Username:"), 0, 0, 1, 1);
        widgets.usernameEntry = gtk_entry_new();
        gtk_grid_attach(GTK_GRID(grid), widgets.usernameEntry, 1, 0, 1, 1);
        
        // Server
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Server:"), 0, 1, 1, 1);
        widgets.serverEntry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(widgets.serverEntry), "127.0.0.1");
        gtk_grid_attach(GTK_GRID(grid), widgets.serverEntry, 1, 1, 1, 1);
        
        // Port
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Port:"), 0, 2, 1, 1);
        widgets.portEntry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(widgets.portEntry), "8080");
        gtk_grid_attach(GTK_GRID(grid), widgets.portEntry, 1, 2, 1, 1);
        
        // Connect button
        widgets.connectBtn = gtk_button_new_with_label("Connect");
        gtk_grid_attach(GTK_GRID(grid), widgets.connectBtn, 0, 3, 2, 1);
    }
    
    void createChatBox(GtkWidget* parent) {
        widgets.chatBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_box_pack_start(GTK_BOX(parent), widgets.chatBox, TRUE, TRUE, 0);
        
        // Main horizontal pane: chat view + online users
        GtkWidget* hBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(widgets.chatBox), hBox, TRUE, TRUE, 0);
        
        // Chat view with scroll (left side)
        GtkWidget* chatScrolled = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_widget_set_hexpand(chatScrolled, TRUE);
        gtk_widget_set_vexpand(chatScrolled, TRUE);
        widgets.chatView = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(widgets.chatView), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(widgets.chatView), GTK_WRAP_WORD);
        widgets.chatBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widgets.chatView));
        gtk_container_add(GTK_CONTAINER(chatScrolled), widgets.chatView);
        gtk_box_pack_start(GTK_BOX(hBox), chatScrolled, TRUE, TRUE, 0);
        
        // Online users panel (right side)
        GtkWidget* usersBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_size_request(usersBox, 150, -1);
        gtk_box_pack_start(GTK_BOX(hBox), usersBox, FALSE, FALSE, 0);
        
        GtkWidget* usersLabel = gtk_label_new("Online Users");
        gtk_widget_set_halign(usersLabel, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(usersBox), usersLabel, FALSE, FALSE, 5);
        
        // Create list store: icon, username
        widgets.onlineUsersStore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
        widgets.onlineUsersView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(widgets.onlineUsersStore));
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(widgets.onlineUsersView), FALSE);
        
        // Icon column (green dot)
        GtkCellRenderer* iconRenderer = gtk_cell_renderer_text_new();
        g_object_set(iconRenderer, "foreground", "green", NULL);
        GtkTreeViewColumn* iconCol = gtk_tree_view_column_new_with_attributes("", iconRenderer, "text", 0, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(widgets.onlineUsersView), iconCol);
        
        // Username column
        GtkCellRenderer* nameRenderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn* nameCol = gtk_tree_view_column_new_with_attributes("User", nameRenderer, "text", 1, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(widgets.onlineUsersView), nameCol);
        
        GtkWidget* usersScrolled = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(usersScrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(usersScrolled), widgets.onlineUsersView);
        gtk_box_pack_start(GTK_BOX(usersBox), usersScrolled, TRUE, TRUE, 0);
        
        // Group controls
        GtkWidget* groupBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(widgets.chatBox), groupBox, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(groupBox), gtk_label_new("Group:"), FALSE, FALSE, 0);
        widgets.groupEntry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(groupBox), widgets.groupEntry, TRUE, TRUE, 0);
        widgets.joinGroupBtn = gtk_button_new_with_label("Join Group");
        gtk_box_pack_start(GTK_BOX(groupBox), widgets.joinGroupBtn, FALSE, FALSE, 0);
        
        // Recipient controls
        GtkWidget* recipientBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(widgets.chatBox), recipientBox, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(recipientBox), gtk_label_new("DM To:"), FALSE, FALSE, 0);
        widgets.recipientEntry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(recipientBox), widgets.recipientEntry, TRUE, TRUE, 0);
        
        // Message controls
        GtkWidget* msgBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_box_pack_start(GTK_BOX(widgets.chatBox), msgBox, FALSE, FALSE, 0);
        widgets.messageEntry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(msgBox), widgets.messageEntry, TRUE, TRUE, 0);
        widgets.sendBtn = gtk_button_new_with_label("Send");
        gtk_box_pack_start(GTK_BOX(msgBox), widgets.sendBtn, FALSE, FALSE, 0);
        widgets.sendFileBtn = gtk_button_new_with_label("Send File");
        gtk_box_pack_start(GTK_BOX(msgBox), widgets.sendFileBtn, FALSE, FALSE, 0);
    }
};

#endif // GUI_HANDLER_H
