#ifndef CARO_GAME_H
#define CARO_GAME_H

#include <gtk/gtk.h>
#include <string>
#include <cstring>
#include <functional>

#define CARO_MAX_SIZE 10
#define CARO_DEFAULT_SIZE 10
#define CARO_MIN_SIZE 3

// Forward declaration
class ChatClient;

// Caro game state
struct CaroState {
    GtkWidget* window;           // Game window
    GtkWidget* grid;             // Board grid
    GtkWidget* statusLabel;      // Status display
    GtkWidget* turnLabel;        // Turn indicator
    GtkWidget* opponentLabel;    // Opponent name
    GtkWidget* winLabel;         // Win banner
    GtkWidget* playAgainBtn;     // Play again button
    GtkWidget* cells[CARO_MAX_SIZE][CARO_MAX_SIZE]; // Grid buttons
    
    char board[CARO_MAX_SIZE][CARO_MAX_SIZE];  // Game board
    int boardSize;               // Current board size (3, 5, 10)
    int winLength;               // How many in a row to win
    
    char mySymbol;               // 'X' or 'O'
    char oppSymbol;              // Opponent's symbol
    bool myTurn;                 // Is it my turn?
    bool inGame;                 // Is game active?
    bool waitingAccept;          // Waiting for opponent to accept invite
    
    std::string opponent;        // Opponent username
    std::string myUsername;      // My username
    
    // Callbacks
    std::function<bool(const std::string&, const std::string&)> sendGameMessage;
    
    CaroState() : window(nullptr), grid(nullptr), statusLabel(nullptr),
                  turnLabel(nullptr), opponentLabel(nullptr), winLabel(nullptr),
                  playAgainBtn(nullptr), boardSize(CARO_DEFAULT_SIZE), winLength(5),
                  mySymbol('X'), oppSymbol('O'), myTurn(false), inGame(false),
                  waitingAccept(false) {
        memset(board, 0, sizeof(board));
        memset(cells, 0, sizeof(cells));
    }
};

// Helper functions
static int caro_required_in_a_row(int size) {
    switch (size) {
        case 3: return 3;
        case 4: return 4;
        case 5: return 5;
        default: return 5;  // 10x10 cần 5
    }
}

static int caro_sanitize_size(int size) {
    if (size == 3 || size == 4 || size == 5 || size == 10) return size;
    return CARO_DEFAULT_SIZE;
}

// Forward declarations for callbacks
static void caro_cell_clicked(GtkButton* button, gpointer user_data);
static gboolean caro_on_window_close(GtkWidget* widget, GdkEvent* event, gpointer user_data);
static void caro_play_again_clicked(GtkButton* button, gpointer user_data);

// Clear cell style
static void caro_clear_cell_style(GtkWidget* btn) {
    if (!btn) return;
    GtkStyleContext* ctx = gtk_widget_get_style_context(btn);
    gtk_style_context_remove_class(ctx, "caro-x");
    gtk_style_context_remove_class(ctx, "caro-o");
    gtk_style_context_remove_class(ctx, "caro-win");
}

// Set cell symbol with style
static void caro_set_cell_symbol(GtkWidget* btn, char symbol) {
    if (!btn) return;
    caro_clear_cell_style(btn);
    char label[2] = { symbol, '\0' };
    gtk_button_set_label(GTK_BUTTON(btn), label);
    GtkStyleContext* ctx = gtk_widget_get_style_context(btn);
    if (symbol == 'X') gtk_style_context_add_class(ctx, "caro-x");
    else if (symbol == 'O') gtk_style_context_add_class(ctx, "caro-o");
}

// Mark winning line
static void caro_mark_win_line(CaroState* state, int startR, int startC, int dr, int dc) {
    if (!state) return;
    for (int i = 0; i < state->winLength; i++) {
        int r = startR + dr * i;
        int c = startC + dc * i;
        if (r < 0 || c < 0 || r >= state->boardSize || c >= state->boardSize) break;
        if (state->cells[r][c]) {
            GtkStyleContext* ctx = gtk_widget_get_style_context(state->cells[r][c]);
            gtk_style_context_add_class(ctx, "caro-win");
        }
    }
}

// Check for win
static bool caro_check_win(CaroState* state, int row, int col, char symbol) {
    int size = state->boardSize;
    int need = state->winLength;
    const int dirs[4][2] = { {1,0}, {0,1}, {1,1}, {1,-1} };
    
    for (int d = 0; d < 4; d++) {
        int count = 1;
        int startR = row, startC = col;
        
        // Count forward
        for (int step = 1; step < need; step++) {
            int r = row + dirs[d][0] * step;
            int c = col + dirs[d][1] * step;
            if (r < 0 || c < 0 || r >= size || c >= size) break;
            if (state->board[r][c] == symbol) count++;
            else break;
        }
        
        // Count backward and track start position
        for (int step = 1; step < need; step++) {
            int r = row - dirs[d][0] * step;
            int c = col - dirs[d][1] * step;
            if (r < 0 || c < 0 || r >= size || c >= size) break;
            if (state->board[r][c] == symbol) {
                count++;
                startR = r;
                startC = c;
            } else break;
        }
        
        if (count >= need) {
            caro_mark_win_line(state, startR, startC, dirs[d][0], dirs[d][1]);
            return true;
        }
    }
    return false;
}

// Check if board is full
static bool caro_board_full(CaroState* state) {
    int size = state->boardSize;
    for (int r = 0; r < size; r++) {
        for (int c = 0; c < size; c++) {
            if (state->board[r][c] == '\0') return false;
        }
    }
    return true;
}

// Update labels
static void caro_update_labels(CaroState* state, const char* status, const char* turn) {
    if (!state) return;
    if (state->statusLabel && status) {
        gtk_label_set_text(GTK_LABEL(state->statusLabel), status);
    }
    if (state->turnLabel && turn) {
        gtk_label_set_text(GTK_LABEL(state->turnLabel), turn);
    }
}

// Show win banner
static void caro_show_win_banner(CaroState* state, const char* text) {
    if (!state || !state->winLabel || !text) return;
    gtk_label_set_text(GTK_LABEL(state->winLabel), text);
    gtk_widget_show(state->winLabel);
}

// Finish game
static void caro_finish_game(CaroState* state, const char* statusText) {
    state->inGame = false;
    state->waitingAccept = false;
    state->myTurn = false;
    caro_update_labels(state, statusText, "🏁 Game finished");
    if (state->playAgainBtn) {
        gtk_widget_set_sensitive(state->playAgainBtn, TRUE);
    }
}

// Reset board
static void caro_reset_board(CaroState* state) {
    if (!state) return;
    int size = state->boardSize > 0 ? state->boardSize : CARO_DEFAULT_SIZE;
    for (int r = 0; r < size; r++) {
        for (int c = 0; c < size; c++) {
            state->board[r][c] = '\0';
            if (state->cells[r][c]) {
                caro_clear_cell_style(state->cells[r][c]);
                gtk_button_set_label(GTK_BUTTON(state->cells[r][c]), " ");
            }
        }
    }
    if (state->winLabel) {
        gtk_widget_hide(state->winLabel);
    }
}

// Cell click handler
static void caro_cell_clicked(GtkButton* button, gpointer user_data) {
    CaroState* state = static_cast<CaroState*>(user_data);
    if (!state) return;
    
    if (!state->inGame || !state->myTurn) {
        return;
    }
    
    int row = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "caro-row"));
    int col = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "caro-col"));
    
    if (row < 0 || col < 0 || row >= state->boardSize || col >= state->boardSize) return;
    if (state->board[row][col] != '\0') return;
    
    // Make move
    state->board[row][col] = state->mySymbol;
    caro_set_cell_symbol(GTK_WIDGET(button), state->mySymbol);
    
    bool win = caro_check_win(state, row, col, state->mySymbol);
    bool draw = (!win) && caro_board_full(state);
    
    // Send move to opponent
    if (state->sendGameMessage) {
        char payload[64];
        snprintf(payload, sizeof(payload), "MOVE|%d|%d", row, col);
        state->sendGameMessage(state->opponent, payload);
    }
    
    if (win) {
        char msg[64];
        snprintf(msg, sizeof(msg), "🎉 You won! %d in a row.", state->winLength);
        caro_finish_game(state, msg);
        caro_show_win_banner(state, (state->mySymbol == 'X') ? "🏆 X Wins!" : "🏆 O Wins!");
        
        if (state->sendGameMessage) {
            state->sendGameMessage(state->opponent, "END|WIN");
        }
    } else if (draw) {
        caro_finish_game(state, "🤝 Draw. Board is full.");
        if (state->winLabel) gtk_widget_hide(state->winLabel);
        
        if (state->sendGameMessage) {
            state->sendGameMessage(state->opponent, "END|DRAW");
        }
    } else {
        state->myTurn = false;
        caro_update_labels(state, nullptr, "Waiting for opponent...");
    }
}

// Create game grid
static void caro_create_grid(CaroState* state) {
    if (!state) return;
    
    int size = caro_sanitize_size(state->boardSize);
    state->boardSize = size;
    state->winLength = caro_required_in_a_row(size);
    
    // Remove old grid if exists
    if (state->grid) {
        gtk_widget_destroy(state->grid);
        state->grid = nullptr;
    }
    
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
    
    int btnSize = (size <= 5) ? 55 : 38;
    
    for (int r = 0; r < size; r++) {
        for (int c = 0; c < size; c++) {
            GtkWidget* btn = gtk_button_new_with_label(" ");
            gtk_widget_set_size_request(btn, btnSize, btnSize);
            gtk_style_context_add_class(gtk_widget_get_style_context(btn), "caro-cell");
            g_object_set_data(G_OBJECT(btn), "caro-row", GINT_TO_POINTER(r));
            g_object_set_data(G_OBJECT(btn), "caro-col", GINT_TO_POINTER(c));
            g_signal_connect(btn, "clicked", G_CALLBACK(caro_cell_clicked), state);
            gtk_grid_attach(GTK_GRID(grid), btn, c, r, 1, 1);
            state->cells[r][c] = btn;
        }
    }
    
    state->grid = grid;
}

// Window close handler
static gboolean caro_on_window_close(GtkWidget* widget, GdkEvent* event, gpointer user_data) {
    (void)event;
    CaroState* state = static_cast<CaroState*>(user_data);
    if (!state) return FALSE;
    
    if (state->inGame || state->waitingAccept) {
        if (!state->opponent.empty() && state->sendGameMessage) {
            state->sendGameMessage(state->opponent, "END|RESIGN");
        }
    }
    
    state->inGame = false;
    state->waitingAccept = false;
    gtk_widget_hide(widget);
    return TRUE;
}

// Play again button handler
static void caro_play_again_clicked(GtkButton* button, gpointer user_data) {
    (void)button;
    CaroState* state = static_cast<CaroState*>(user_data);
    if (!state) return;
    
    if (state->opponent.empty()) return;
    
    // Send invite again
    if (state->sendGameMessage) {
        char payload[64];
        snprintf(payload, sizeof(payload), "INVITE|%d", state->boardSize);
        state->sendGameMessage(state->opponent, payload);
        state->waitingAccept = true;
        caro_update_labels(state, "Invite sent. Waiting for response...", "");
        gtk_widget_set_sensitive(state->playAgainBtn, FALSE);
    }
}

// Create game window
static void caro_create_window(CaroState* state) {
    if (state->window) return;
    
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "🎮 Caro Game");
    gtk_window_set_default_size(GTK_WINDOW(window), 520, 600);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    
    // Add CSS for beautiful styling
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        /* Window background */
        "window { background: linear-gradient(180deg, #1a1a2e 0%, #16213e 100%); }"
        
        /* Main container */
        ".main-box { background: transparent; }"
        
        /* Header labels */
        ".header-label { color: #eee; font-size: 14px; font-weight: bold; }"
        ".opponent-label { color: #00d4ff; font-size: 16px; font-weight: bold; }"
        ".status-label { color: #ffd700; font-size: 13px; }"
        ".turn-label { color: #00ff88; font-size: 15px; font-weight: bold; }"
        
        /* Game grid frame */
        ".game-frame { background: #0f3460; border-radius: 10px; padding: 10px; }"
        
        /* Cell buttons */
        ".caro-cell { "
        "   background: linear-gradient(145deg, #1a1a2e, #16213e); "
        "   border: 2px solid #0f3460; "
        "   border-radius: 8px; "
        "   font-size: 20px; "
        "   font-weight: bold; "
        "   min-width: 40px; "
        "   min-height: 40px; "
        "   transition: all 0.2s; "
        "}"
        ".caro-cell:hover { "
        "   background: linear-gradient(145deg, #2a2a4e, #26315e); "
        "   border-color: #00d4ff; "
        "}"
        
        /* X symbol - Blue gradient */
        ".caro-x { "
        "   color: #00d4ff; "
        "   text-shadow: 0 0 10px #00d4ff, 0 0 20px #00d4ff; "
        "   font-weight: bold; "
        "   font-size: 22px; "
        "}"
        
        /* O symbol - Pink/Red gradient */
        ".caro-o { "
        "   color: #ff6b9d; "
        "   text-shadow: 0 0 10px #ff6b9d, 0 0 20px #ff6b9d; "
        "   font-weight: bold; "
        "   font-size: 22px; "
        "}"
        
        /* Winning cells */
        ".caro-win { "
        "   background: linear-gradient(145deg, #ffd700, #ffaa00) !important; "
        "   border-color: #ffd700 !important; "
        "   animation: pulse 0.5s infinite alternate; "
        "}"
        
        /* Win banner */
        ".win-banner { "
        "   background: rgba(0, 0, 0, 0.85); "
        "   color: #ffd700; "
        "   font-size: 28px; "
        "   font-weight: bold; "
        "   padding: 20px 40px; "
        "   border-radius: 15px; "
        "   border: 3px solid #ffd700; "
        "   text-shadow: 0 0 10px #ffd700; "
        "}"
        
        /* Buttons */
        ".game-btn { "
        "   background: linear-gradient(145deg, #e94560, #c73e54); "
        "   color: white; "
        "   border: none; "
        "   border-radius: 8px; "
        "   padding: 10px 25px; "
        "   font-weight: bold; "
        "   font-size: 13px; "
        "}"
        ".game-btn:hover { "
        "   background: linear-gradient(145deg, #ff5577, #e94560); "
        "}"
        ".game-btn:disabled { "
        "   background: #555; "
        "   color: #888; "
        "}"
        
        ".play-again-btn { "
        "   background: linear-gradient(145deg, #00d4ff, #00a8cc); "
        "}"
        ".play-again-btn:hover { "
        "   background: linear-gradient(145deg, #00e8ff, #00d4ff); "
        "}",
        -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);
    
    GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_style_context_add_class(gtk_widget_get_style_context(mainBox), "main-box");
    gtk_container_set_border_width(GTK_CONTAINER(mainBox), 15);
    gtk_container_add(GTK_CONTAINER(window), mainBox);
    
    // Header
    state->opponentLabel = gtk_label_new("👤 Opponent: -");
    gtk_style_context_add_class(gtk_widget_get_style_context(state->opponentLabel), "opponent-label");
    gtk_box_pack_start(GTK_BOX(mainBox), state->opponentLabel, FALSE, FALSE, 0);
    
    state->statusLabel = gtk_label_new("⏳ Not in game");
    gtk_style_context_add_class(gtk_widget_get_style_context(state->statusLabel), "status-label");
    gtk_box_pack_start(GTK_BOX(mainBox), state->statusLabel, FALSE, FALSE, 0);
    
    state->turnLabel = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(state->turnLabel), "turn-label");
    gtk_box_pack_start(GTK_BOX(mainBox), state->turnLabel, FALSE, FALSE, 0);
    
    // Grid container with overlay for win banner
    GtkWidget* overlay = gtk_overlay_new();
    gtk_box_pack_start(GTK_BOX(mainBox), overlay, TRUE, TRUE, 0);
    
    // Create grid
    caro_create_grid(state);
    
    GtkWidget* gridFrame = gtk_frame_new(nullptr);
    gtk_style_context_add_class(gtk_widget_get_style_context(gridFrame), "game-frame");
    gtk_container_add(GTK_CONTAINER(gridFrame), state->grid);
    gtk_container_add(GTK_CONTAINER(overlay), gridFrame);
    
    // Win label overlay
    state->winLabel = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(state->winLabel), "win-banner");
    gtk_widget_set_halign(state->winLabel, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(state->winLabel, GTK_ALIGN_CENTER);
    gtk_widget_set_no_show_all(state->winLabel, TRUE);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), state->winLabel);
    
    // Bottom buttons
    GtkWidget* btnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_widget_set_halign(btnBox, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(mainBox), btnBox, FALSE, FALSE, 10);
    
    state->playAgainBtn = gtk_button_new_with_label("🔄 Play Again");
    gtk_style_context_add_class(gtk_widget_get_style_context(state->playAgainBtn), "game-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(state->playAgainBtn), "play-again-btn");
    gtk_widget_set_sensitive(state->playAgainBtn, FALSE);
    g_signal_connect(state->playAgainBtn, "clicked", G_CALLBACK(caro_play_again_clicked), state);
    gtk_box_pack_start(GTK_BOX(btnBox), state->playAgainBtn, FALSE, FALSE, 0);
    
    GtkWidget* leaveBtn = gtk_button_new_with_label("🚪 Leave Game");
    gtk_style_context_add_class(gtk_widget_get_style_context(leaveBtn), "game-btn");
    g_signal_connect(leaveBtn, "clicked", G_CALLBACK(caro_on_window_close), state);
    gtk_box_pack_start(GTK_BOX(btnBox), leaveBtn, FALSE, FALSE, 0);
    
    g_signal_connect(window, "delete-event", G_CALLBACK(caro_on_window_close), state);
    
    state->window = window;
}

// Start a game match
static void caro_start_match(CaroState* state, const std::string& opponent, bool startFirst, char mySymbol, int boardSize) {
    state->opponent = opponent;
    state->inGame = true;
    state->waitingAccept = false;
    state->myTurn = startFirst;
    state->mySymbol = mySymbol;
    state->oppSymbol = (mySymbol == 'X') ? 'O' : 'X';
    state->boardSize = caro_sanitize_size(boardSize);
    state->winLength = caro_required_in_a_row(state->boardSize);
    
    caro_create_window(state);
    
    // Recreate grid for new size
    if (state->grid) {
        GtkWidget* parent = gtk_widget_get_parent(state->grid);
        if (parent) {
            gtk_container_remove(GTK_CONTAINER(parent), state->grid);
        }
        state->grid = nullptr;
    }
    caro_create_grid(state);
    
    // Add grid back to frame
    GList* children = gtk_container_get_children(GTK_CONTAINER(state->window));
    if (children) {
        GtkWidget* mainBox = GTK_WIDGET(children->data);
        GList* mainChildren = gtk_container_get_children(GTK_CONTAINER(mainBox));
        for (GList* l = mainChildren; l; l = l->next) {
            if (GTK_IS_OVERLAY(l->data)) {
                GList* overlayChildren = gtk_container_get_children(GTK_CONTAINER(l->data));
                if (overlayChildren && GTK_IS_FRAME(overlayChildren->data)) {
                    gtk_container_add(GTK_CONTAINER(overlayChildren->data), state->grid);
                }
                g_list_free(overlayChildren);
                break;
            }
        }
        g_list_free(mainChildren);
        g_list_free(children);
    }
    
    caro_reset_board(state);
    
    char oppText[128];
    snprintf(oppText, sizeof(oppText), "👤 Opponent: %s", opponent.c_str());
    gtk_label_set_text(GTK_LABEL(state->opponentLabel), oppText);
    
    char statusText[128];
    snprintf(statusText, sizeof(statusText), "🎮 Game %dx%d - Need %d in a row", 
             state->boardSize, state->boardSize, state->winLength);
    gtk_label_set_text(GTK_LABEL(state->statusLabel), statusText);
    
    gtk_label_set_text(GTK_LABEL(state->turnLabel), 
                       startFirst ? "✨ Your turn (X)" : "⏳ Waiting for opponent...");
    
    gtk_widget_set_sensitive(state->playAgainBtn, FALSE);
    if (state->winLabel) gtk_widget_hide(state->winLabel);
    
    gtk_widget_show_all(state->window);
    gtk_window_present(GTK_WINDOW(state->window));
}

// Handle incoming game invite
static void caro_handle_invite(CaroState* state, const std::string& from, int boardSize) {
    if (state->inGame || state->waitingAccept) {
        // Busy, decline
        if (state->sendGameMessage) {
            state->sendGameMessage(from, "DECLINE|BUSY");
        }
        return;
    }
    
    state->boardSize = caro_sanitize_size(boardSize);
    state->winLength = caro_required_in_a_row(state->boardSize);
    
    char prompt[256];
    snprintf(prompt, sizeof(prompt), 
             "%s invites you to play Caro %dx%d\n(Need %d in a row to win)\n\nAccept?",
             from.c_str(), state->boardSize, state->boardSize, state->winLength);
    
    GtkWidget* dialog = gtk_message_dialog_new(nullptr,
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               "%s", prompt);
    gtk_dialog_add_buttons(GTK_DIALOG(dialog), 
                           "Decline", GTK_RESPONSE_REJECT,
                           "Accept", GTK_RESPONSE_ACCEPT,
                           nullptr);
    gint resp = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (resp == GTK_RESPONSE_ACCEPT) {
        if (state->sendGameMessage) {
            char payload[64];
            snprintf(payload, sizeof(payload), "ACCEPT|%d", state->boardSize);
            state->sendGameMessage(from, payload);
        }
        caro_start_match(state, from, false, 'O', state->boardSize);
    } else {
        if (state->sendGameMessage) {
            state->sendGameMessage(from, "DECLINE|NO");
        }
    }
}

// Handle invite response
static void caro_handle_accept(CaroState* state, const std::string& from, const std::string& response, int boardSize) {
    if (!state->waitingAccept || state->opponent != from) {
        return;
    }
    
    if (response == "ACCEPT") {
        caro_start_match(state, from, true, 'X', boardSize);
    } else {
        state->waitingAccept = false;
        if (state->playAgainBtn) {
            gtk_widget_set_sensitive(state->playAgainBtn, TRUE);
        }
        
        GtkWidget* dialog = gtk_message_dialog_new(nullptr,
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   "Game invite was declined.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

// Handle opponent move
static void caro_handle_move(CaroState* state, const std::string& from, int row, int col) {
    if (!state->inGame || state->opponent != from) {
        return;
    }
    
    if (row < 0 || col < 0 || row >= state->boardSize || col >= state->boardSize) return;
    if (state->board[row][col] != '\0') return;
    
    state->board[row][col] = state->oppSymbol;
    if (state->cells[row][col]) {
        caro_set_cell_symbol(state->cells[row][col], state->oppSymbol);
    }
    
    if (caro_check_win(state, row, col, state->oppSymbol)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "😢 You lost! Opponent made %d in a row.", state->winLength);
        caro_finish_game(state, msg);
        caro_show_win_banner(state, (state->oppSymbol == 'X') ? "🏆 X Wins!" : "🏆 O Wins!");
        return;
    }
    
    if (caro_board_full(state)) {
        caro_finish_game(state, "🤝 Draw. Board is full.");
        return;
    }
    
    state->myTurn = true;
    caro_update_labels(state, nullptr, "✨ Your turn");
}

// Handle game end
static void caro_handle_end(CaroState* state, const std::string& from, const std::string& reason) {
    if (state->opponent != from) return;
    
    if (reason == "WIN") {
        caro_finish_game(state, "😢 You lost!");
        caro_show_win_banner(state, (state->oppSymbol == 'X') ? "🏆 X Wins!" : "🏆 O Wins!");
    } else if (reason == "DRAW") {
        caro_finish_game(state, "🤝 Draw. Board is full.");
    } else if (reason == "RESIGN") {
        caro_finish_game(state, "🎉 Opponent resigned. You win!");
        caro_show_win_banner(state, (state->mySymbol == 'X') ? "🏆 X Wins!" : "🏆 O Wins!");
    } else {
        caro_finish_game(state, "Game ended.");
    }
}

// Parse and handle game message
static void caro_handle_message(CaroState* state, const std::string& from, const std::string& payload) {
    // Parse payload: TYPE|param1|param2|...
    size_t pos1 = payload.find('|');
    std::string type = (pos1 != std::string::npos) ? payload.substr(0, pos1) : payload;
    std::string rest = (pos1 != std::string::npos) ? payload.substr(pos1 + 1) : "";
    
    if (type == "INVITE") {
        int size = CARO_DEFAULT_SIZE;
        if (!rest.empty()) size = atoi(rest.c_str());
        caro_handle_invite(state, from, size);
    }
    else if (type == "ACCEPT") {
        int size = CARO_DEFAULT_SIZE;
        if (!rest.empty()) size = atoi(rest.c_str());
        caro_handle_accept(state, from, "ACCEPT", size);
    }
    else if (type == "DECLINE") {
        caro_handle_accept(state, from, "DECLINE", 0);
    }
    else if (type == "MOVE") {
        // Parse row|col
        size_t pos2 = rest.find('|');
        if (pos2 != std::string::npos) {
            int row = atoi(rest.substr(0, pos2).c_str());
            int col = atoi(rest.substr(pos2 + 1).c_str());
            caro_handle_move(state, from, row, col);
        }
    }
    else if (type == "END") {
        caro_handle_end(state, from, rest);
    }
}

#endif // CARO_GAME_H
