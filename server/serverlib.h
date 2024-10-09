#ifndef SERVERLIB_H
#define SERVERLIB_H

#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 2000

struct client_info {
    int socket;
    struct sockaddr_in address;
    int id;
};

void *handle_client(void *arg);
void send_to_client(int client_id, const char *message);
void handle_terminal_input();
void accept_clients(int socket_desc, struct sockaddr_in *server_addr);
void setup_server(struct sockaddr_in *server_addr, int *socket_desc);
void cleanup_client(struct client_info *client);

#endif
