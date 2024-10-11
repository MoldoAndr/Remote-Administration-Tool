#ifndef CLIENT_DAEMON_H
#define CLIENT_DAEMON_H

#define BUFFER_SIZE 2048
#define LOG_FILE_NAME "LogFile.txt"

#include <stddef.h>

void get_current_time(char *buffer, int buffer_size);
void log_command(const char *server_message);
void process_server_command(const char *server_message, char *response);
void daemon_init();
void get_executable_path(char *argv0);
void connect_to_server(const char *server_ip, int server_port);
void handle_server_messages();
void signal_handler(int sig);
void get_username_and_station_name(char *station_info, size_t size);

#endif

