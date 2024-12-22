#include "client_daemon.h"

int client_socket;
char executable_path[MAX_PATH];
char server_IP[IP_BUFFER_SIZE];

int send_packet(int socket, const struct DataPacket *packet)
{
    /* Revised to ensure the entire struct is sent, even if 'send()' returns partial. */
    size_t total_sent = 0;
    const char *ptr = (const char *)packet;
    size_t length = sizeof(*packet);

    while (total_sent < length)
    {
        ssize_t bytes_sent = send(socket, ptr + total_sent, length - total_sent, 0);
        if (bytes_sent < 0)
        {
            perror("Failed to send packet");
            return -1;
        }
        total_sent += bytes_sent;
    }
    return 0;
}

int receive_packet(int socket, struct DataPacket *packet)
{
    size_t total_received = 0;
    char *ptr = (char *)packet;
    size_t length = sizeof(*packet);

    while (total_received < length)
    {
        ssize_t bytes_received = recv(socket, ptr + total_received, length - total_received, 0);
        if (bytes_received <= 0)
        {
            return -1;
        }
        total_received += bytes_received;
    }
    log_command(packet->data);
    return 0;
}

void authenticate_with_server(int client_socket)
{
    struct DataPacket packet;
    char token_file[BUFFER_SIZE];
    char token[37] = {0};
    bool token_exists = false;

    snprintf(token_file, sizeof(token_file), "%s/%s", executable_path, TOKEN_FILENAME);
    int fd = open(token_file, O_RDONLY);

    if (fd >= 0)
    {
        token_exists = true;
        read(fd, token, sizeof(token));
        close(fd);
    }

    memset(&packet, 0, sizeof(packet));
    packet.type = TEXT;
    get_username_and_station_name(packet.data, sizeof(packet.data));
    if (token_exists)
    {
        strcat(packet.data, " ");
        strcat(packet.data, token);
    }

    if (send_packet(client_socket, &packet) < 0)
    {
        syslog(LOG_ERR, "Failed to send authentication packet");
        exit(EXIT_FAILURE);
    }

    if (!token_exists)
    {
        if (receive_packet(client_socket, &packet) == 0 && packet.type == TEXT)
        {
            strncpy(token, packet.data, sizeof(token) - 1);
            fd = open(token_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0)
            {
                write(fd, token, strlen(token));
                close(fd);
            }
            printf("Token received and saved: %s\n", token);
        }
    }
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

    // Authenticate with server using packets
    authenticate_with_server(client_socket);

    set_monitor_active();
    set_alert_active();
    return true;
}

void send_file_to_server(const char *file_path)
{
    FILE *file = fopen(file_path, "rb");
    if (!file)
    {
        perror("Error opening file");
        return;
    }

    struct DataPacket packet;
    memset(&packet, 0, sizeof(packet));
    packet.type = FILE_TRANSFER;
    strncpy(packet.filename, file_path, MAX_FILENAME_SIZE);

    size_t bytes_read;
    while ((bytes_read = fread(packet.data, 1, CHUNK_SIZE, file)) > 0)
    {
        packet.data_size = bytes_read;
        if (send_packet(client_socket, &packet) < 0)
        {
            perror("Error sending file packet");
            fclose(file);
            return;
        }
        memset(packet.data, 0, CHUNK_SIZE);
    }

    printf("File '%s' sent to server.\n", file_path);
    fclose(file);
}

void handle_server_messages()
{
    struct DataPacket packet;
    struct DataPacket response_packet;
    bool first_message = true;

    while (1)
    {
        memset(&packet, 0, sizeof(packet));
        memset(&response_packet, 0, sizeof(response_packet));
        if (receive_packet(client_socket, &packet) < 0)
        {
            syslog(LOG_ERR, "Error receiving DataPacket from server");
            break;
        }
        if (packet.data_size == 0)
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
            process_server_command(packet.data, response_packet.data);
        }
        log_command(packet.data);
        response_packet.type = TEXT;
        response_packet.data_size = strlen(response_packet.data);
        if (send_packet(client_socket, &response_packet) < 0)
        {
            syslog(LOG_ERR, "Error sending response DataPacket to server");
            break;
        }
    }
    close(client_socket);
}
