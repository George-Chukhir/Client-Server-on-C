# Compiler and flags 
CC = gcc
CFLAGS = -Wall -pthread

# Default target
all: server client

# Server compilation
server: server.c
	$(CC) $(CFLAGS) -o server server.c

# Client compilation
client: client.c
	$(CC) $(CFLAGS) -o client client.c

# Deleting compiled files
clean:
	rm -f server client
