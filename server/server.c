#include "serverlib.h"

int main(int argc, char* argv[]) 
{
    int socket_desc;
    struct sockaddr_in server_addr;
    if (argc < 2)
    {
        write(1,"Server IP address MUST be specified\n",36);
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {
        write(2,"Error handling SIGINT\n",23);
        exit(EXIT_FAILURE);
    }
    
    setup_server(&server_addr, &socket_desc,argv[1]);

    pthread_t terminal_thread;
    
    if (pthread_create(&terminal_thread, NULL, (void *)handle_terminal_input, NULL) != 0) 
    {
        perror("Failed to create terminal thread");
        return 1;
    }
    
    accept_clients(socket_desc, &server_addr);

    close(socket_desc);
    return 0;
}
