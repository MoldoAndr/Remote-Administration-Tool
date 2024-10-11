#include "serverlib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>
#include <fcntl.h>

#if 1

pthread_mutex_t client_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_count = 0;
struct client_info *clients[MAX_CLIENTS];
char *log_folder = "client_logs";

#endif

void setup_server(struct sockaddr_in *server_addr, int *socket_desc) 
{
    struct stat st = {0};
    *socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if (stat(log_folder, &st) == -1) 
    {
        mkdir(log_folder, 0700);
    }


    if (*socket_desc < 0) 
    {
        perror("Error while creating socket");
        exit(EXIT_FAILURE);
    }
    
    write(1,"Socket created successfully\n", 29);

    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(12345);
    server_addr->sin_addr.s_addr = inet_addr("127.0.0.1");

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
    
    printf("\nListening for incoming connections.....\n");
}

void log_command(const char *client_ip, int client_port, const char* station_name, const char *command) 
{
    char log_filename[BUFFER_SIZE];

    snprintf(log_filename, sizeof(log_filename), "%s/%s_%d_%s.log", log_folder, client_ip, client_port, station_name);

    int log_fd = open(log_filename, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (log_fd < 0) 
    {
        write(1, "Could not open log file\n", 24);
        return;
    }

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    char time_buffer[BUFFER_SIZE];

    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", local_time);

    char log_entry[BUFFER_SIZE];
    int log_entry_len = snprintf(log_entry, sizeof(log_entry), "Date/Time: %s - Command: %s\n", time_buffer, command);

    if (log_entry_len > 0) 
    {
        write(log_fd, log_entry, log_entry_len);
    }

    close(log_fd);
}

void accept_clients(int socket_desc, struct sockaddr_in *server_addr) 
{
    int client_size;
    pthread_t thread_id;

    while (1) 
    {
        struct client_info *client = malloc(sizeof(struct client_info));
        client_size = sizeof(client->address);
        client->socket = accept(socket_desc, (struct sockaddr *)&client->address, &client_size);

        if (client->socket < 0) 
        {
            printf("Can't accept\n");
            free(client);
            continue;
        }

        pthread_mutex_lock(&client_count_mutex);
        client->id = ++client_count;
        pthread_mutex_unlock(&client_count_mutex);

        printf("client%d connected at IP: %s and port: %i\n", 
               client->id, inet_ntoa(client->address.sin_addr), ntohs(client->address.sin_port));

        pthread_mutex_lock(&clients_mutex);
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
    
    if (client_id > 0 && client_id <= MAX_CLIENTS && clients[client_id - 1] != NULL) {
        struct client_info *client = clients[client_id - 1];

        if (send(client->socket, message, strlen(message), 0) < 0) 
        {
            printf("Failed to send message to client%d\n", client_id);
        } 
        else 
        {
            log_command(inet_ntoa(client->address.sin_addr), ntohs(client->address.sin_port), client->station_info, message);
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
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];
    char client_id_str[20];
    bool has_printed_message = false;

    snprintf(client_id_str, sizeof(client_id_str), "client%d", client->id);

    recv(client->socket, client_message, sizeof(client_message), 0);

    printf("Received station_info from %s: %s\n", client_id_str, client_message);

    strcpy(client->station_info, client_message);

    while (1) {
        memset(server_message, '\0', sizeof(server_message));
        memset(client_message, '\0', sizeof(client_message));

        if (recv(client->socket, client_message, sizeof(client_message), 0) < 0) {
            printf("Couldn't receive from %s\n", client_id_str);
            break;
        }

        if (strlen(client_message) == 0) {
            printf("%s disconnected\n", client_id_str);
            break;
        }

        if (!has_printed_message) 
        {
            printf("Msg from %s: %s\n", client_id_str, client_message);
            has_printed_message = true;
        }
        
        snprintf(server_message, sizeof(server_message), "Server received your message, %s.", client_id_str);
        
        if (send(client->socket, server_message, strlen(server_message), 0) < 0) {
            printf("Can't send to %s\n", client_id_str);
            break;
        }
    }

    cleanup_client(client);
    pthread_exit(NULL);
}

void cleanup_client(struct client_info *client) 
{
    pthread_mutex_lock(&clients_mutex);
    clients[client->id - 1] = NULL;
    pthread_mutex_unlock(&clients_mutex);

    close(client->socket);
    free(client);
}

void handle_terminal_input() 
{
    char input[BUFFER_SIZE];
    while (1) 
    {
        printf("Enter command (clientX:command) or 'exit' to quit: \n");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Error reading input. Exiting.\n");
            break;
        }

        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "exit") == 0) {
            printf("Exiting terminal input handler.\n");
            break;
        }

        if (strncmp(input, "client", 6) != 0) {
            printf("Invalid command format. Use clientX:command\n");
            continue;
        }

        char *colon = strchr(input, ':');
        if (colon == NULL) {
            printf("Invalid command format. Use clientX:command\n");
            continue;
        }

        *colon = '\0';
        int client_id = atoi(input + 6);
        if (client_id <= 0) {
            printf("Invalid client ID. Must be a positive number.\n");
            continue;
        }

        char *command = colon + 1;
        if (strlen(command) == 0) {
            printf("Command cannot be empty.\n");
            continue;
        }

        if(sizeof(input))
        {
            send_to_client(client_id, command);
        }
        printf("Command sent to client%d: %s\n", client_id, command);

        input[0]='\0';
    }
}