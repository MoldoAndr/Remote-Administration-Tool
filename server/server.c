#include "serverlib.h"
#include <stdio.h>
#include <pthread.h>

int main(void) 
{
    int socket_desc;
    struct sockaddr_in server_addr;

    setup_server(&server_addr, &socket_desc);

    pthread_t terminal_thread;
    
    if (pthread_create(&terminal_thread, NULL, (void *)handle_terminal_input, NULL) != 0) 
    {
        perror("Failed to create terminal thread");
        return 1;
    }
    
    accept_clients(socket_desc, &server_addr);

    pclose(socket_desc);
    return 0;
}
