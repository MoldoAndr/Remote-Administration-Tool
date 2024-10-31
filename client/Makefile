CLIENT = client

run_client: 
	gcc -c client_daemon.c -o client_daemon.o
	ar rcs libclient_daemon.a client_daemon.o
	gcc main.c -L. -lclient_daemon -o client
	./client $(server_IP) 12345

clean:
	rm -f $(CLIENT)
	rm -f client_daemon.o
	rm -f libclient_daemon.a
	rm -f client_token.txt
