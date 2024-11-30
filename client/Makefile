CLIENT = client
CC = gcc
AR = ar
CFLAGS = -Wall -Wextra  $$(pkg-config --cflags --libs glib-2.0) -lpcap
PORT = 12345
IP ?= 127.0.0.1
NR ?= 1 

OBJS = client_monitor.o network.o process_command.o setup.o utility.o mongoose.o monitor.o domain_alert.o
LIB = libclient_daemon.a

.PHONY: all clean dependencies run_client run_clients

all: dependencies $(CLIENT)

dependencies:
	@echo "Installing dependencies..."
	@sudo apt install -y libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev gnome-screenshot libpcap-dev libglib2.0-dev
	@echo "Dependencies installed."

$(CLIENT): main.c $(LIB)
	$(CC) $(CFLAGS) $< -L. -lclient_daemon -o $@

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run_client: $(CLIENT) dependencies
	@if [ -z "$(IP)" ]; then \
		echo "Error: IP address not provided. Use: make run_client IP=<ip_address>"; \
		exit 1; \
	fi
	sudo ./$(CLIENT) $(IP) $(PORT)

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
	rm -f LogFile.txt

client_monitor.o: client_monitor.c
network.o: network.c
process_command.o: process_command.c
setup.o: setup.c
utility.o: utility.c
mongoose.o: mongoose.c
monitor.o: monitor.c
domain_alert.o: domain_alert.c
