# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -g
# OpenSSL flags (pkg-config preferred, fallback to -lssl -lcrypto)
OPENSSL_CFLAGS ?= $(shell pkg-config --cflags openssl 2>/dev/null)
OPENSSL_LIBS ?= $(shell pkg-config --libs openssl 2>/dev/null)
ifeq ($(OPENSSL_LIBS),)
OPENSSL_LIBS = -lssl -lcrypto
endif
CXXFLAGS += $(OPENSSL_CFLAGS)

CLIENT_LDFLAGS = -pthread -lsodium $(OPENSSL_LIBS)
SERVER_LDFLAGS = -pthread -lsqlite3 -lsodium $(OPENSSL_LIBS)

# Directories
CLIENT_DIR = client
SERVER_DIR = server
COMMON_DIR = common
DATABASE_DIR = database
AUTH_DIR = auth
CERT_DIR = cert

# Output executables
CLIENT_BIN = $(CLIENT_DIR)/client
SERVER_BIN = $(SERVER_DIR)/server

# Certificate and key files (updated to .pem)
CERT_KEY = $(CERT_DIR)/server-key.pem
CERT_CRT = $(CERT_DIR)/server-cert.pem

# Recursive wildcard function to find all .cpp files
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# Find all .cpp files recursively (strip ./ prefix for proper filtering)
ALL_SRC = $(patsubst ./%,%,$(call rwildcard,.,*.cpp))

# Separate client and server sources
CLIENT_SRC = $(filter $(CLIENT_DIR)/%,$(ALL_SRC))
SERVER_SRC = $(filter $(SERVER_DIR)/%,$(ALL_SRC))

# Find common sources - shared by both client and server
COMMON_SRC = $(filter $(COMMON_DIR)/%,$(ALL_SRC))

# Find database sources - only for server
DATABASE_SRC = $(filter $(DATABASE_DIR)/%,$(ALL_SRC))

# Find auth sources - only for server
AUTH_SRC = $(filter $(AUTH_DIR)/%,$(ALL_SRC))

# Object files - convert .cpp to .o
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
COMMON_OBJ = $(COMMON_SRC:.cpp=.o)
DATABASE_OBJ = $(DATABASE_SRC:.cpp=.o)
AUTH_OBJ = $(AUTH_SRC:.cpp=.o)

# Get all unique directories for include paths
ALL_DIRS = $(sort $(dir $(ALL_SRC)))
INCLUDE_FLAGS = $(addprefix -I,$(ALL_DIRS))

# Default target - build both client and server
all: $(CLIENT_BIN) $(SERVER_BIN)

# Build client - client sources + common sources
$(CLIENT_BIN): $(CLIENT_OBJ) $(COMMON_OBJ)
	$(CXX) -o $@ $^ $(CLIENT_LDFLAGS)

# Build server - server sources + common sources + auth + database
$(SERVER_BIN): $(SERVER_OBJ) $(COMMON_OBJ) $(DATABASE_OBJ) $(AUTH_OBJ)
	$(CXX) -o $@ $^ $(SERVER_LDFLAGS)

# Generic rule to compile any .cpp file
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE_FLAGS) -c $< -o $@

# Build only client
client: $(CLIENT_BIN)

# Build only server
server: $(SERVER_BIN)

# Clean build artifacts
clean:
	rm -f $(CLIENT_OBJ) $(SERVER_OBJ) $(COMMON_OBJ) $(DATABASE_OBJ) $(AUTH_OBJ) $(CLIENT_BIN) $(SERVER_BIN)

# Clean and rebuild
rebuild: clean all

# Run client
run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

# Run server
run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

# Generate self-signed server certificate and key
cert:
	mkdir -p $(CERT_DIR)
	openssl req -newkey rsa:4096 -nodes -keyout $(CERT_KEY) -x509 -days 365 -out $(CERT_CRT) -subj "/CN=localhost"
	chmod 600 $(CERT_KEY)
	chmod 644 $(CERT_CRT)
	@echo "Generated certificates:"
	@echo "  Private key: $(CERT_KEY)"
	@echo "  Certificate: $(CERT_CRT)"

# Debug: print what we found (run with 'make debug')
debug:
	@echo "CLIENT_SRC: $(CLIENT_SRC)"
	@echo "SERVER_SRC: $(SERVER_SRC)"
	@echo "COMMON_SRC: $(COMMON_SRC)"
	@echo "DATABASE_SRC: $(DATABASE_SRC)"
	@echo "AUTH_SRC: $(AUTH_SRC)"
	@echo "CLIENT_OBJ: $(CLIENT_OBJ)"
	@echo "SERVER_OBJ: $(SERVER_OBJ)"
	@echo "COMMON_OBJ: $(COMMON_OBJ)"
	@echo "DATABASE_OBJ: $(DATABASE_OBJ)"
	@echo "AUTH_OBJ: $(AUTH_OBJ)"

.PHONY: all client server clean rebuild run-client run-server debug cert