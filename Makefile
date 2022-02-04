CC = gcc
CFLAGS = -Wall -Wextra -g

.PHONY: all clean

all: client server

client: client.o

server: server.o

client.o: client.c

server.o: server.c

clean:
	-rm -f *.o *~ client server *_client
