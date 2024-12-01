#ifndef CLIENT_DAEMON_H
#define CLIENT_DAEMON_H

#define BUFFER_SIZE 2048
#define MAX_PATH 256
#define LOG_FILE_NAME "LogFile.txt"
#define TOKEN_FILENAME "client_token.txt"
#define IP_BUFFER_SIZE 64
#define PORT 52577
#define MAX_ARGS 100
#define MAX_BLOCKED_DOMAINS 1000
#define MAX_DOMAIN_LENGTH 256

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
#include <regex.h>
#include <glib.h>
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

extern int client_socket;
extern char executable_path[MAX_PATH];
extern char server_IP[IP_BUFFER_SIZE];
extern char blocked_domains[MAX_BLOCKED_DOMAINS][MAX_DOMAIN_LENGTH];
extern int blocked_count;
extern pthread_t alert_thread;
extern int stop_alert_thread;

/* Initialization and setup functions */
void daemon_init();
void get_executable_path();
void signal_handler(int);

/* Network communication functions */
bool connect_to_server(const char *, int);
void authenticate_with_server(int);
void handle_server_messages();

/* Command processing functions */
void process_server_command(const char *, char *);
int parse_command(char *, char *, char **);
int execute_command(const char *, char **, char *);
void cleanup_args(char *);

/* Monitoring functions */
void *execute_monitor(void *);
int handle_monitor_command(char *);
void run_system_monitor_server(char *, int, time_t);
void set_monitor_active();

/* Utility functions */
void get_current_time(char *, int);
void log_command(const char *);
int get_system_ip(char *, size_t);
void get_username_and_station_name(char *, size_t);

/* Domain access alert*/
int load_blocked_domains(const char *filename);
int is_domain_blocked(const char *domain);
void *log_network_access();
void set_alert_active();
void stop_alert();

#endif
