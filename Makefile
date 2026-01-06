# Chat App Makefile for Cross-Platform (Windows/Linux)

CXX = g++
CXXFLAGS = -std=c++11 -Wall -I.

# GTK flags
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

# Detect OS
ifeq ($(OS),Windows_NT)
    # Windows (MSYS2/MinGW)
    LIBS_SERVER = -lws2_32
    LIBS_CLIENT = -lws2_32 $(GTK_LIBS)
    EXE_EXT = .exe
    MKDIR = mkdir -p
else
    # Linux/Unix
    LIBS_SERVER = -lpthread
    LIBS_CLIENT = -lpthread $(GTK_LIBS)
    EXE_EXT =
    MKDIR = mkdir -p
endif

# Directories
BIN_DIR = bin

# Targets
SERVER = $(BIN_DIR)/server$(EXE_EXT)
CLIENT = $(BIN_DIR)/client$(EXE_EXT)

.PHONY: all server client clean directories

all: directories server client

directories:
	$(MKDIR) $(BIN_DIR)

server: directories
	$(CXX) $(CXXFLAGS) -o $(SERVER) socket_server/server_main.cpp $(LIBS_SERVER)
	@echo "Server built: $(SERVER)"

client: directories
	$(CXX) $(CXXFLAGS) $(GTK_CFLAGS) -o $(CLIENT) socket_client/client_main.cpp $(LIBS_CLIENT)
	@echo "Client built: $(CLIENT)"

clean:
	rm -rf $(BIN_DIR)

run-server: server
	./$(SERVER)

run-client: client
	./$(CLIENT)
