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
#include <sys/ioctl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>

#define MAX_CLIENTS 256
#define BUFFER_SIZE 2048
#define MAX_NUMBERS 10

extern char **commands;

extern pthread_mutex_t client_count_mutex;
extern pthread_mutex_t clients_mutex;

extern int client_count;
extern struct client_info *clients[MAX_CLIENTS];
extern char *log_folder;

extern int active_client_numbers[MAX_CLIENTS];
extern int num_active_clients;

struct client_info
{
    int socket;
    struct sockaddr_in address;
    int id;
    char station_info[MAX_CLIENTS];
};

// Token Functions
void generate_token(char *token);
void store_token(char *station_name, char *token);
bool validate_token(const char *client_info, const char *received_token);

// Server Terminal Handler Function
void handle_terminal_input();
void list_clients();
void print_logs();
void list_commands();
bool valid(char *input);
void cleanup_and_exit();
bool handle_client_command(const char *input);
bool handle_builtin_commands(const char *input);
char *trim(char *str);
void info();

// Client Handle and Communication Functions
void *handle_client(void *arg);
void send_to_client(int client_id, const char *message);
void accept_clients(int, struct sockaddr_in *);
void setup_server(struct sockaddr_in *, int *, char *);
void cleanup_client(struct client_info *);
void send_to_client_list(const char *client_list, const char *command);
bool client_exists(int client_no);
bool compare_in_addr(const struct in_addr *addr1, const struct in_addr *addr2);

// Command Logger Function
void log_command(const char *station_name, const char *command, const char *output);

// Command Completion Functions
char **custom_completion(const char *text, int start, int end);
char *command_generator(const char *text, int state);
void initialize_commands();
void add_command(const char *new_command);
int delete_command(const char *command_to_delete);
void free_commands();

// Monitor Clients Statistics
char *process_clients_statistics();
float *get_metrics(const char *url);
int extract_numbers(const char *str, float *numbers);
void write_statistics();
void *background_writer();
char *format_data(float *data);

#endif