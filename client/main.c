#include "client_daemon.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

int main(int argc, char *argv[]) 
{
    if (argc < 3) 
    {
        write(1, "Usage: ./client <server_ip> <server_port>\n", 41);
        return -1;
    }   
    
    signal(SIGTERM, signal_handler);

    daemon_init();

    get_executable_path(argv[0]);

    connect_to_server(argv[1], atoi(argv[2]));

    handle_server_messages();
}

