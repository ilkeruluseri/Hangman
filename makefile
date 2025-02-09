# Compiler and flags
CC = gcc
CFLAGS = -g

# Targets
SERVER = server
CLIENT = client

# Source files
SERVER_SRC = server.c
CLIENT_SRC = client.c

# Header files
HEADERS = utils.h

# Default target
all: $(SERVER) $(CLIENT)

# Build server
$(SERVER): $(SERVER_SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $^

# Build client
$(CLIENT): $(CLIENT_SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $^

# Clean up binaries
clean:
	rm -f $(SERVER) $(CLIENT)

# Phony targets
.PHONY: all clean
