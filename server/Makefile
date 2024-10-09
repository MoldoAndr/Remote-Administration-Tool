CC = gcc
CFLAGS = -Wall -fPIC
LDFLAGS = -shared
TARGET_LIB = libserverlib.so
TARGET_EXEC = server

all: $(TARGET_LIB) $(TARGET_EXEC)

$(TARGET_LIB): serverlib.o
	$(CC) $(LDFLAGS) -o $(TARGET_LIB) serverlib.o

serverlib.o: serverlib.c serverlib.h
	$(CC) $(CFLAGS) -c serverlib.c -o serverlib.o

$(TARGET_EXEC): server.o
	$(CC) -o $(TARGET_EXEC) server.o -L. -lserverlib -lpthread

server.o: server.c serverlib.h
	$(CC) $(CFLAGS) -c server.c -o server.o

run: $(TARGET_LIB) $(TARGET_EXEC)
	LD_LIBRARY_PATH=. ./server

clean:
	rm -f *.o $(TARGET_LIB) $(TARGET_EXEC)
