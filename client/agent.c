#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFER_SIZE 2000

const char *client_info = "Client Name: ClientX\nID: 12345\n";

void process_server_command(const char *server_message, char *response) {
    if (strncmp(server_message, "REQUEST_INFO", 12) == 0) {
        snprintf(response, BUFFER_SIZE, "Here is my info:\n%s", client_info);
    } else if (strncmp(server_message, "REQUEST_STATUS", 14) == 0) {
        snprintf(response, BUFFER_SIZE, "Status: All systems operational!");
    } else {
        snprintf(response, BUFFER_SIZE, "Echoing server message: %s", server_message);
    }
}

void create_daemon() {
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) {
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);
}

int main() {
    create_daemon();
    
    int socket_desc;
    struct sockaddr_in server_addr;
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc < 0) {
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        return -1;
    }

    while (1) {
        memset(server_message, '\0', sizeof(server_message));
        memset(client_message, '\0', sizeof(client_message));

        if (recv(socket_desc, server_message, sizeof(server_message), 0) < 0) {
            break;
        }

        if (strncmp(server_message, "REQUEST_", 8) == 0) {
            char response[BUFFER_SIZE];
            process_server_command(server_message, response);

            if (send(socket_desc, response, strlen(response), 0) < 0) {
                break;
            }
        }
    }

    close(socket_desc);
    return 0;
}

