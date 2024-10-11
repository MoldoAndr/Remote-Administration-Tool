CLIENT = client

run_client:
	gcc -c client_daemon.c -o client_daemon.o
	ar rcs libclient_daemon.a client_daemon.o
	gcc main.c -L. -lclient_daemon -o client
	./client 127.0.0.1 12345

run_clients:
	@echo "Starting clients..."
	for i in $(shell seq 1 10); do \
		./$(CLIENT) 127.0.0.1 & \
	done

clean:
	rm -f $(CLIENT)
	rm -f client_daemon.o
	rm -f libclient_daemon.a

