# Compiler
CC = gcc

# Compiler flags
# -pthread is required for the server (and good practice for IPC)
CFLAGS = -pthread -Wall

# Targets
all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client *.o
