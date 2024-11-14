#ifndef CLIENT_DAEMON_H
#define CLIENT_DAEMON_H

#define BUFFER_SIZE 2048
#define MAX_PATH 256
#define LOG_FILE_NAME "LogFile.txt"
#define TOKEN_FILENAME "client_token.txt"
#define IP_BUFFER_SIZE 16
#define PORT 52577
#define MAX_ARGS 100

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <uuid/uuid.h>
#include <libgen.h>
#include <pwd.h>
#include <stdbool.h>

typedef struct
{
    char ip[IP_BUFFER_SIZE];
    int port;
    time_t duration;
} monitor_args_t;

void get_current_time(char *, int);
void log_command(const char *);
void *execute_monitor(void *);
int get_system_ip(char *, size_t );
int handle_monitor_command(const char *, char *);
void process_server_command(const char *, char *);
int parse_command(char *, char *, char **);
int execute_command(const char *, char **, char *);
void cleanup_args(char *);
void daemon_init();
void get_executable_path();
void authenticate_with_server(int);
bool connect_to_server(const char *, int);
void handle_server_messages();
void signal_handler(int);
void get_username_and_station_name(char *, size_t);

void run_system_monitor_server(const char *, int, int);


#endif
