CC=gcc
CFLAGS=-Wall -Wextra -std=gnu99 -g

all: client server


client: client.c commonfunctions.c
	$(CC) $(CFLAGS) client.c commonfunctions.c -o client

server: server.c commonfunctions.c
	$(CC) $(CFLAGS) server.c commonfunctions.c -o server


clean:
	rm -f client
