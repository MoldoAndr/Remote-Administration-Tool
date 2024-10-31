#ifndef CLIENT_DAEMON_H
#define CLIENT_DAEMON_H

#define BUFFER_SIZE 2048
#define LOG_FILE_NAME "LogFile.txt"
#define TOKEN_FILENAME "client_token.txt"

#include <stddef.h>

void    authenticate_with_server(int);
void    get_current_time(char *, int);
void    log_command(const char *);
void    process_server_command(const char *, char *);
void    daemon_init();
void    get_executable_path(char *);
void    connect_to_server(const char *, int);
void    handle_server_messages();
void    signal_handler(int);
void    get_username_and_station_name(char *, size_t);

#endif

