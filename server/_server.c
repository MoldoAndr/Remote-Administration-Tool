#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 2000

struct client_info {
    int socket;
    struct sockaddr_in address;
    int id;
};

pthread_mutex_t client_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_count = 0;
struct client_info *clients[MAX_CLIENTS];

void *handle_client(void *arg);
void send_to_client(int client_id, const char *message);
void handle_terminal_input();
void accept_clients(int socket_desc, struct sockaddr_in *server_addr);
void setup_server(struct sockaddr_in *server_addr, int *socket_desc);
void cleanup_client(struct client_info *client);

int main(void) {
    int socket_desc;
    struct sockaddr_in server_addr;
    printf("Enter command (clientX:command) or 'exit' to quit: ");
    setup_server(&server_addr, &socket_desc);

    pthread_t terminal_thread;
    if (pthread_create(&terminal_thread, NULL, (void *)handle_terminal_input, NULL) != 0) {
        perror("Failed to create terminal thread");
        return 1;
    }

    accept_clients(socket_desc, &server_addr);

    close(socket_desc);
    return 0;
}

void setup_server(struct sockaddr_in *server_addr, int *socket_desc) {
    *socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (*socket_desc < 0) {
        printf("Error while creating socket\n");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");

    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(2000);
    server_addr->sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(*socket_desc, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        printf("Couldn't bind to the port\n");
        exit(EXIT_FAILURE);
    }
    printf("Done with binding\n");

    if (listen(*socket_desc, MAX_CLIENTS) < 0) {
        printf("Error while listening\n");
        exit(EXIT_FAILURE);
    }
    printf("\nListening for incoming connections.....\n");
}

void accept_clients(int socket_desc, struct sockaddr_in *server_addr) {
    int client_size;
    pthread_t thread_id;

    while (1) {
        struct client_info *client = malloc(sizeof(struct client_info));
        client_size = sizeof(client->address);
        client->socket = accept(socket_desc, (struct sockaddr *)&client->address, &client_size);

        if (client->socket < 0) {
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

        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) < 0) {
            printf("Could not create thread\n");
            free(client);
            continue;
        }

        pthread_detach(thread_id);
    }
}

void send_to_client(int client_id, const char *message) {
    pthread_mutex_lock(&clients_mutex);
    if (client_id > 0 && client_id <= MAX_CLIENTS && clients[client_id - 1] != NULL) {
        if (send(clients[client_id - 1]->socket, message, strlen(message), 0) < 0) {
            printf("Failed to send message to client%d\n", client_id);
        } else {
            printf("Sent message to client%d: %s\n", client_id, message);
        }
    } else {
        printf("Client%d not found or disconnected\n", client_id);
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    struct client_info *client = (struct client_info *)arg;
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];
    char client_id_str[20];

    snprintf(client_id_str, sizeof(client_id_str), "client%d", client->id);

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

        printf("Msg from %s: %s\n", client_id_str, client_message);

        snprintf(server_message, sizeof(server_message), "Server received your message, %s.", client_id_str);
        if (send(client->socket, server_message, strlen(server_message), 0) < 0) {
            printf("Can't send to %s\n", client_id_str);
            break;
        }
    }

    cleanup_client(client);
    pthread_exit(NULL);
}

void cleanup_client(struct client_info *client) {
    pthread_mutex_lock(&clients_mutex);
    clients[client->id - 1] = NULL;
    pthread_mutex_unlock(&clients_mutex);

    close(client->socket);
    free(client);
}

void handle_terminal_input() {
    char input[BUFFER_SIZE];
    while (1) {
        printf("Enter command (clientX:command) or 'exit' to quit: ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "exit") == 0) {
            break;
        }

        int client_id = atoi(&input[6]);
        char *command = strchr(input, ':');
        if (command != NULL) {
            command++;
            send_to_client(client_id, command);
        } else {
            printf("Invalid command format. Use clientX:command\n");
        }
    }
}
