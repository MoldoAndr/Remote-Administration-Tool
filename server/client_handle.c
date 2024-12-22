#include "serverlib.h"

bool compare_in_addr(const struct in_addr *addr1, const struct in_addr *addr2)
{
    if (addr1->s_addr == addr2->s_addr)
        return true;
    return false;
}

bool client_exists(int client_no)
{
    for (int i = 0; i < client_count; ++i)
    {
        if (clients[i] && clients[i]->id == client_no)
            return true;
    }
    return false;
}

int send_packet(int socket, struct DataPacket *packet)
{
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

int recv_packet(int socket, struct DataPacket *packet)
{
    size_t total_received = 0;
    char *ptr = (char *)packet;
    size_t length = sizeof(*packet);

    while (total_received < length)
    {
        ssize_t bytes = recv(socket, ptr + total_received, length - total_received, 0);
        if (bytes <= 0)
        {
            return -1;
        }
        total_received += bytes;
    }
    return 0;
}

bool already_connected(struct sockaddr_in *info)
{
    for (int i = 0; i < client_count; ++i)
    {
        if (clients[i] != NULL && compare_in_addr(&clients[i]->address.sin_addr, &info->sin_addr))
            return true;
    }
    return false;
}

void accept_clients(int socket_desc, struct sockaddr_in *server_addr)
{
    socklen_t client_size;
    pthread_t thread_id;
    while (1)
    {
        struct client_info *client = malloc(sizeof(struct client_info));

        memset(client, 0, sizeof(struct client_info));

        client_size = sizeof(client->address);
        client->socket = accept(socket_desc, (struct sockaddr *)&client->address, &client_size);

        if (client->socket < 0)
        {
            printf("Can't accept\n");
            free(client);
            continue;
        }

        snprintf(client->station_info, sizeof(client->station_info),
                 "%s:%d",
                 inet_ntoa(client->address.sin_addr),
                 ntohs(client->address.sin_port));

        if (already_connected(&client->address))
        {
            printf("A client that is already connected: %s, tries to connect\nBlocked!\n",
                   client->station_info);
            free(client);
            continue;
        }

        pthread_mutex_lock(&client_count_mutex);
        client->id = ++client_count;
        pthread_mutex_unlock(&client_count_mutex);

        char client_string[64];
        snprintf(client_string, 64, "client%d:", client->id);
        add_command(client_string);

        printf("client%d connected at IP: %s and port: %i\n",
               client->id, inet_ntoa(client->address.sin_addr), ntohs(client->address.sin_port));

        pthread_mutex_lock(&clients_mutex);
        if (client->id > 0)
            clients[client->id - 1] = client;
        pthread_mutex_unlock(&clients_mutex);

        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) < 0)
        {
            printf("Could not create thread\n");
            free(client);
            continue;
        }

        pthread_detach(thread_id);
    }
}

void send_to_client(int client_id, const char *message)
{
    pthread_mutex_lock(&clients_mutex);

    if (client_id > 0 && client_id <= MAX_CLIENTS && clients[client_id - 1] != NULL)
    {
        struct client_info *client = clients[client_id - 1];
        struct DataPacket packet;

        memset(&packet, 0, sizeof(packet));
        packet.type = TEXT;
        strncpy(packet.data, message, CHUNK_SIZE);
        packet.data_size = strlen(packet.data);

        if (send_packet(client->socket, &packet) < 0)
        {
            printf("Failed to send message to client%d\n", client_id);
        }
        else
        {
            printf("Message sent to client%d: %s\n", client_id, message);
        }
    }
    else
    {
        printf("Client%d not found or disconnected\n", client_id);
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg)
{
    struct client_info *client = (struct client_info *)arg;
    struct DataPacket packet;

    if (recv_packet(client->socket, &packet) < 0)
    {
        printf("Client disconnected before initial message, client%d\n", client->id);
        cleanup_client(client);
        return NULL;
    }

    printf("Received initial raw data from client%d: %s\n\n", client->id, packet.data);

    if (!strchr(packet.data, ' '))
    {
        char token[37];
        generate_token(token);

        struct DataPacket token_packet;
        memset(&token_packet, 0, sizeof(token_packet));
        token_packet.type = TEXT;
        strncpy(token_packet.data, token, CHUNK_SIZE - 1);
        token_packet.data_size = strlen(token_packet.data);

        if (send_packet(client->socket, &token_packet) < 0)
        {
            printf("Failed to send token to client%d\n", client->id);
            cleanup_client(client);
            return NULL;
        }

        store_token(packet.data, token);
    }
    else
    {
        char copy_data[BUFFER_SIZE];
        strncpy(copy_data, packet.data, sizeof(copy_data) - 1);
        copy_data[sizeof(copy_data) - 1] = '\0';

        char *station_name = strtok(copy_data, " ");
        char *received_token = strtok(NULL, " ");

        if (!validate_token(station_name, received_token))
        {
            printf("Invalid token received from client%d\n", client->id);
            cleanup_client(client);
            return NULL;
        }
        else
        {
            printf("Token validated for client%d\n", client->id);
        }
    }

    strncpy(client->station_info, packet.data, sizeof(client->station_info) - 1);
    client->station_info[sizeof(client->station_info) - 1] = '\0';

    {
        struct DataPacket ack_packet;
        memset(&ack_packet, 0, sizeof(ack_packet));
        ack_packet.type = TEXT;
        snprintf(ack_packet.data, CHUNK_SIZE, "Server received your station info.");
        ack_packet.data_size = strlen(ack_packet.data);

        if (send_packet(client->socket, &ack_packet) < 0)
        {
            printf("Failed to send station info acknowledgment to client%d\n", client->id);
            cleanup_client(client);
            return NULL;
        }
    }

    while (1)
    {
        memset(&packet, 0, sizeof(packet));

        if (recv_packet(client->socket, &packet) < 0)
        {
            printf("Client disconnected: %s\n", client->station_info);
            cleanup_client(client);
            return NULL;
        }

        if (packet.type == TEXT)
        {
            printf("Received text from client%d: %s\n", client->id, packet.data);

            if (strncmp(packet.data, "exited with success", 19) == 0)
            {
                printf("Client %s disconnected gracefully.\n", client->station_info);
                cleanup_client(client);
                return NULL;
            }

            char server_message[BUFFER_SIZE] = {0};
            log_command(client->station_info, server_message, packet.data);
        }
        else if (packet.type == FILE_TRANSFER)
        {
            printf("Receiving file '%s' from client%d\n", packet.filename, client->id);
            FILE *file = fopen(packet.filename, "wb");
            if (!file)
            {
                perror("Error creating file");
                continue;
            }
            fwrite(packet.data, 1, packet.data_size, file);
            fclose(file);
            printf("File '%s' saved successfully.\n", packet.filename);
        }
    }

    cleanup_client(client);
    return NULL;
}


void send_file_to_client(int client_id, const char *file_path)
{
    pthread_mutex_lock(&clients_mutex);

    if (client_id > 0 && client_id <= MAX_CLIENTS && clients[client_id - 1] != NULL)
    {
        struct client_info *client = clients[client_id - 1];
        FILE *file = fopen(file_path, "rb");
        if (!file)
        {
            printf("Error opening file: %s\n", file_path);
            pthread_mutex_unlock(&clients_mutex);
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
            if (send_packet(client->socket, &packet) < 0)
            {
                printf("Failed to send file chunk to client%d\n", client_id);
                break;
            }
            memset(packet.data, 0, CHUNK_SIZE);
        }

        fclose(file);
        printf("File '%s' sent to client%d\n", file_path, client_id);
    }
    else
    {
        printf("Client%d not found or disconnected\n", client_id);
    }

    pthread_mutex_unlock(&clients_mutex);
}

void cleanup_client(struct client_info *client)
{
    if (client == NULL)
        return;
    pthread_mutex_lock(&clients_mutex);
    if (client->id > 0 && client->id <= MAX_CLIENTS)
    {
        char client_delete[64];
        snprintf(client_delete, sizeof(client_delete), "client%d:", client->id);
        delete_command(client_delete);
        clients[client->id - 1] = NULL;
    }
    else
    {
        printf("Invalid client ID: %d\n", client->id);
    }

    pthread_mutex_unlock(&clients_mutex);
    if (client->socket >= 0)
    {
        close(client->socket);
    }
    if (client)
    {
        free(client);
        client = NULL;
    }
}

void send_to_client_list(const char *client_list, const char *command)
{
    char *list_copy = strdup(client_list);
    char *token = strtok(list_copy, ",");

    while (token != NULL)
    {
        char *dash = strchr(token, '-');
        if (dash)
        {
            *dash = '\0';
            int start = atoi(token);
            int end = atoi(dash + 1);

            if (start > 0 && end > 0 && start <= MAX_CLIENTS && end <= MAX_CLIENTS)
            {
                for (int i = start; i <= end; i++)
                {
                    send_to_client(i, command);
                    printf("Command sent to client%d: %s\n", i, command);
                }
            }
            else
            {
                printf("Invalid client range: %s-%s\n", token, dash + 1);
            }
        }
        else
        {
            int client_id = atoi(token);
            if (client_id > 0 && client_id <= MAX_CLIENTS)
            {
                send_to_client(client_id, command);
                printf("Command sent to client%d: %s\n", client_id, command);
            }
            else
            {
                printf("Invalid client ID: %s\n", token);
            }
        }
        token = strtok(NULL, ",");
    }

    free(list_copy);
}

void setup_server(struct sockaddr_in *server_addr, int *socket_desc, char *server_address)
{
    struct stat st = {0};
    *socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    initialize_commands();

    if (stat(log_folder, &st) == -1)
    {
        mkdir(log_folder, 0700);
    }

    if (*socket_desc < 0)
    {
        perror("Error while creating socket");
        exit(EXIT_FAILURE);
    }

    write(1, "Socket created successfully\n", 29);

    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(12345);
    server_addr->sin_addr.s_addr = inet_addr(server_address);

    if (bind(*socket_desc, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        perror("Couldn't bind to the port");
        exit(EXIT_FAILURE);
    }

    printf("Done with binding\n");

    if (listen(*socket_desc, MAX_CLIENTS) < 0)
    {
        perror("Error while listening");
        exit(EXIT_FAILURE);
    }

    pthread_t thread_id;

    if (pthread_create(&thread_id, NULL, background_writer, NULL) < 0)
    {
        printf("Could not create thread\n");
        return;
    }

    printf("Listening for incoming connections.....\n");
}
