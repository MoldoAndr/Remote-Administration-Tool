#include "client_daemon.h"

int client_socket;
char executable_path[MAX_PATH];
char server_IP[IP_BUFFER_SIZE];

int send_data(int socket, const char *data, size_t length) {
    ssize_t bytes_sent = send(socket, data, length, 0);
    if (bytes_sent < 0) {
        perror("Failed to send data");
        return -1;
    }
    return 0;
}

int receive_data(int socket, char *buffer, size_t max_length) {
    ssize_t bytes_received = recv(socket, buffer, max_length - 1, 0);
    if (bytes_received <= 0) {
        return -1;
    }
    buffer[bytes_received] = '\0';
    log_command(buffer);
    return bytes_received;
}

void authenticate_with_server(int client_socket) {
    char auth_buffer[BUFFER_SIZE] = {0};
    char token_file[BUFFER_SIZE];
    char token[37] = {0};
    bool token_exists = false;

    snprintf(token_file, sizeof(token_file), "%s/%s", executable_path, TOKEN_FILENAME);
    int fd = open(token_file, O_RDONLY);

    if (fd >= 0) {
        token_exists = true;
        read(fd, token, sizeof(token));
        close(fd);
    }

    get_username_and_station_name(auth_buffer, sizeof(auth_buffer));
    if (token_exists) {
        strcat(auth_buffer, " ");
        strcat(auth_buffer, token);
    }

    if (send_data(client_socket, auth_buffer, strlen(auth_buffer)) < 0) {
        syslog(LOG_ERR, "Failed to send authentication data");
        exit(EXIT_FAILURE);
    }

    if (!token_exists) {
        char received_token[BUFFER_SIZE] = {0};
        if (receive_data(client_socket, received_token, sizeof(received_token)) > 0) {
            strncpy(token, received_token, sizeof(token) - 1);
            fd = open(token_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                write(fd, token, strlen(token));
                close(fd);
            }
            printf("Token received and saved: %s\n", token);
        }
    }
}

bool connect_to_server(const char *server_ip, int server_port) {
    struct sockaddr_in server_addr;
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0) {
        syslog(LOG_ERR, "Error creating socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    strcpy(server_IP, server_ip);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Error connecting to server");
        close(client_socket);
        exit(EXIT_FAILURE);
        return false;
    }

    syslog(LOG_INFO, "Connected to server at %s:%d", server_ip, server_port);
    authenticate_with_server(client_socket);

    set_monitor_active();
    set_alert_active();
    return true;
}

void send_file_to_server(const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("Error opening file");
        return;
    }

    char buffer[CHUNK_SIZE];
    size_t bytes_read;
    
    send_data(client_socket, file_path, strlen(file_path));
    
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        if (send_data(client_socket, buffer, bytes_read) < 0) {
            perror("Error sending file data");
            fclose(file);
            return;
        }
    }

    printf("File '%s' sent to server.\n", file_path);
    fclose(file);
}

void handle_server_messages() {
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    bool first_message = true;

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        memset(response, 0, BUFFER_SIZE);
        
        if (receive_data(client_socket, buffer, BUFFER_SIZE) < 0) {
            syslog(LOG_ERR, "Error receiving data from server");
            break;
        }
        
        if (strlen(buffer) == 0) {
            syslog(LOG_INFO, "Server disconnected.");
            break;
        }

        if (first_message) {
            first_message = false;
        } else {
            process_server_command(buffer, response);
        }
        
        log_command(buffer);
        
        if (send_data(client_socket, response, strlen(response)) < 0) {
            syslog(LOG_ERR, "Error sending response to server");
            break;
        }
    }
    close(client_socket);
}