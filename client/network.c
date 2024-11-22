#include "client_daemon.h"

int client_socket;
char executable_path[MAX_PATH];
char server_IP[IP_BUFFER_SIZE];

void authenticate_with_server(int client_socket)
{
    char client_info[BUFFER_SIZE];
    char token_file[BUFFER_SIZE];
    char token[37] = {0};
    bool token_exists = false;
    snprintf(token_file, sizeof(token_file), "%s/%s", executable_path, TOKEN_FILENAME);
    get_username_and_station_name(client_info, sizeof(client_info));

    int fd = open(token_file, O_RDONLY);
    if (fd >= 0)
    {
        token_exists = true;
        read(fd, token, sizeof(token));
        close(fd);
        strcat(client_info, " ");
        strcat(client_info, token);
    }
    send(client_socket, client_info, strlen(client_info), 0);
    syslog(LOG_ERR, "SENT CLIENT INFO\n");
    if (!token_exists)
    {
        if (recv(client_socket, token, sizeof(token), 0) > 0)
        {
            log_command("Am primit token");
            fd = open(token_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0)
            {
                write(fd, token, strlen(token));
                close(fd);
            }
            send(client_socket, "client received token.\n", strlen("client2 received token.\n"), 0);
        }
    }
    close(fd);
}

bool connect_to_server(const char *server_ip, int server_port)
{
    struct sockaddr_in server_addr;
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0)
    {
        syslog(LOG_ERR, "Error creating socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    strcpy(server_IP, server_ip);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        syslog(LOG_ERR, "Error connecting to server");
        close(client_socket);
        exit(EXIT_FAILURE);
        return false;
    }

    syslog(LOG_INFO, "Connected to server at %s:%d", server_ip, server_port);
    authenticate_with_server(client_socket);
    set_monitor_active();
    return true;
}

void handle_server_messages()
{
    char server_message[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    bool first_message = true;

    while (1)
    {
        memset(server_message, 0, BUFFER_SIZE);
        memset(response, 0, BUFFER_SIZE);

        if (recv(client_socket, server_message, sizeof(server_message), 0) < 0)
        {
            syslog(LOG_ERR, "Error receiving message from server");
            break;
        }

        if (strlen(server_message) == 0)
        {
            syslog(LOG_INFO, "Server disconnected.");
            break;
        }
        if (first_message)
        {
            first_message = false;
        }
        else
        {
            process_server_command(server_message, response);
        }
        log_command(server_message);
        if (send(client_socket, response, strlen(response), 0) < 0)
        {
            syslog(LOG_ERR, "Error sending response to server");
            break;
        }
    }
    close(client_socket);
}
