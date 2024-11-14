# Variables
CLIENT = client
CC = gcc
AR = ar
CFLAGS = -Wall -Wextra
PORT = 12345
IP ?= 127.0.0.1    # Default IP if not provided
NR ?= 1            # Default number of clients if not provided

OBJS = client_daemon.o mongoose.o monitor.o
LIB = libclient_daemon.a

.PHONY: all clean run_client run_clients

all: $(CLIENT)

$(CLIENT): main.c $(LIB)
	$(CC) $(CFLAGS) $< -L. -lclient_daemon -o $@

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run_client: $(CLIENT)
	@if [ -z "$(IP)" ]; then \
		echo "Error: IP address not provided. Use: make run_client IP=<ip_address>"; \
		exit 1; \
	fi
	./$(CLIENT) $(IP) $(PORT)

run_clients: $(CLIENT)
	@if [ -z "$(IP)" ]; then \
		echo "Error: IP address not provided. Use: make run_clients IP=<ip_address> NR=<number>"; \
		exit 1; \
	fi
	@for i in $$(seq 1 $(NR)); do \
		sleep 0.1; \
		./$(CLIENT) $(IP) $(PORT) & \
	done

clean:
	rm -f $(CLIENT)
	rm -f $(OBJS)
	rm -f $(LIB)
	rm -f client_token.txt

client_daemon.o: client_daemon.c
mongoose.o: mongoose.c
monitor.o: monitor.c
