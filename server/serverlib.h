#ifndef SERVERLIB_H
#define SERVERLIB_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <uuid/uuid.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_CLIENTS 64
#define BUFFER_SIZE 2048

char **commands = NULL;

struct client_info
{
    int socket;
    struct sockaddr_in address;
    int id;
    char station_info[BUFFER_SIZE];
};

int active_client_numbers[MAX_CLIENTS];
int num_active_clients = 0;

void send_to_client_list(const char *client_list, const char *command);
void generate_token(char *token);
void store_token(char *station_name, char *token);
bool validate_token(const char *client_info, const char *received_token);
void *handle_client(void *);
void send_to_client(int, const char *);
void handle_terminal_input();
void accept_clients(int, struct sockaddr_in *);
void setup_server(struct sockaddr_in *, int *, char *);
void cleanup_client(struct client_info *);
void log_command(const char *, int, const char *, const char *);
char **custom_completion(const char *text, int start, int end);
char *command_generator(const char *text, int state);
void initialize_commands();
void add_command(const char* new_command);
int delete_command(const char *command_to_delete);
void free_commands();

#endif