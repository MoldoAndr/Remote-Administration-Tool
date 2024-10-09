CC = gcc

CFLAGS = -lpthread

CLIENT = client

CLIENT_SRC = agent.c

all: $(CLIENT)

$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CLIENT_SRC) -o $(CLIENT) $(CFLAGS)

# 10 clients
run_clients:
	@echo "Starting clients..."
	for i in $(shell seq 1 10); do \
		./$(CLIENT) & \
	done

clean:
	rm -f $(CLIENT)

