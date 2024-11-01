CLIENT = client

run_client: 
	gcc -c client_daemon.c -o client_daemon.o
	gcc -c mongoose.c -o mongoose.o
	gcc -c monitor.c -o monitor.o
	ar rcs libclient_daemon.a client_daemon.o mongoose.o monitor.o
	gcc main.c -L. -lclient_daemon -o client
	./client $(IP) 12345

clean:
	rm -f $(CLIENT)
	rm -f *.o
	rm -f libclient_daemon.a
	rm -f client_token.txt
