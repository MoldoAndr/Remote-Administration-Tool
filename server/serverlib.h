#ifndef SERVERLIB_H
#define SERVERLIB_H

#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 2048

struct client_info 
{
    int     socket;
    struct  sockaddr_in address;
    int     id;
    char    station_info[BUFFER_SIZE];
};

void    *handle_client(void *);
void    send_to_client(int , const char *);
void    handle_terminal_input();
void    accept_clients(int , struct sockaddr_in *);
void    setup_server(struct sockaddr_in *, int *);
void    cleanup_client(struct client_info *);
void    log_command(const char *, int , const char*, const char *);

#endif