CC = gcc

CFLAGS = -lpthread

SERVER = server

SERVER_SRC = server.c

all: $(SERVER)

$(SERVER): $(SERVER_SRC)
	$(CC) $(SERVER_SRC) -o $(SERVER) $(CFLAGS)

clean:
	rm -f $(SERVER) 
