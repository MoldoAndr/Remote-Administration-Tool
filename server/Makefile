CC = gcc
CFLAGS = -Wall -fPIC -g
LDFLAGS = -shared
TARGET_LIB = libserverlib.so
TARGET_EXEC = server

LIB_SRCS = client_handle.c command_completion.c logger.c stat_monitor.c terminal_handler.c token_functions.c global_vars.c
LIB_OBJS = $(LIB_SRCS:.c=.o)

all: $(TARGET_LIB) $(TARGET_EXEC)

$(TARGET_LIB): $(LIB_OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET_LIB) $(LIB_OBJS)

%.o: %.c serverlib.h
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_EXEC): server.o $(TARGET_LIB)
	$(CC) -o $(TARGET_EXEC) server.o -L. -lserverlib -lpthread -luuid -lreadline -lefence

server.o: server.c serverlib.h
	$(CC) $(CFLAGS) -c server.c -o server.o

run: $(TARGET_LIB) $(TARGET_EXEC)
	@echo "Checking for processes on port 12345..."
	-sudo kill -9 $$(sudo lsof -t -i:12345) 2>/dev/null || echo "No process found on port 12345."
	LD_LIBRARY_PATH=. ./server $$(hostname --all-ip-addresses)

memcheck: $(TARGET_EXEC)
	@echo "Running Valgrind memcheck..."
	LD_LIBRARY_PATH=. valgrind --leak-check=full --show-leak-kinds=all --tool=memcheck --leak-check=full --error-exitcode=1 ./server $$(hostname --all-ip-addresses)

clean:
	rm -f *.o $(TARGET_LIB) $(TARGET_EXEC)
	rm -f client_tokens.txt client_stats.txt
	rm -rf client_logs

.PHONY: all clean run memcheck

