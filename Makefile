# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
LDFLAGS = -pthread

# Directories
CLIENT_DIR = client
SERVER_DIR = server
NETWORK_DIR = network

# Output executables
CLIENT_BIN = $(CLIENT_DIR)/client
SERVER_BIN = $(SERVER_DIR)/server

# Source files - automatically find all .cpp files
CLIENT_SRC = $(wildcard $(CLIENT_DIR)/*.cpp)
SERVER_SRC = $(wildcard $(SERVER_DIR)/*.cpp)
NETWORK_SRC = $(wildcard $(NETWORK_DIR)/*.cpp)

# Object files - convert .cpp to .o
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
NETWORK_OBJ = $(NETWORK_SRC:.cpp=.o)

# Default target - build both client and server
all: $(CLIENT_BIN) $(SERVER_BIN)

# Build client
$(CLIENT_BIN): $(CLIENT_OBJ) $(NETWORK_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^

# Build server
$(SERVER_BIN): $(SERVER_OBJ) $(NETWORK_OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^

# Compile any .cpp file in client directory
$(CLIENT_DIR)/%.o: $(CLIENT_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(NETWORK_DIR) -I$(SERVER_DIR) -c $< -o $@

# Compile any .cpp file in server directory
$(SERVER_DIR)/%.o: $(SERVER_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(NETWORK_DIR) -I$(SERVER_DIR) -c $< -o $@

# Compile any .cpp file in network directory
$(NETWORK_DIR)/%.o: $(NETWORK_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build only client
client: $(CLIENT_BIN)

# Build only server
server: $(SERVER_BIN)

# Clean build artifacts
clean:
	rm -f $(CLIENT_OBJ) $(SERVER_OBJ) $(NETWORK_OBJ) $(CLIENT_BIN) $(SERVER_BIN)

# Clean and rebuild
rebuild: clean all

# Run client
run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

# Run server
run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

.PHONY: all client server clean rebuild run-client run-server