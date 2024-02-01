CC=gcc
CFLAGS=-I.
DEPS = proxy.h
OBJ = proxy.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) -pthread

compile: client server

client: $(OBJ) client.o 
	$(CC) -o client $(OBJ) client.o

server: $(OBJ) server.o 
	$(CC) -o server $(OBJ) server.o

clean:
	rm -f client
	rm -f server