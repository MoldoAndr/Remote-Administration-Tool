#include "client_daemon.h"
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
#include <netinet/in.h>
#include <libgen.h>
#include <pwd.h>

int client_socket;
char executable_path[BUFFER_SIZE];

void get_current_time(char *buffer, int buffer_size)
{
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", local_time);
}

void log_command(const char *server_message)
{
    char log_file_path[BUFFER_SIZE];

    snprintf(log_file_path, sizeof(log_file_path), "%s/%s", executable_path, LOG_FILE_NAME);

    int log_fd = open(log_file_path, O_WRONLY | O_APPEND | O_CREAT, 0644);

    if (log_fd < 0)
        return;

    char time_str[100];
    get_current_time(time_str, sizeof(time_str));

    char log_entry[BUFFER_SIZE];
    snprintf(log_entry, sizeof(log_entry), "Date/Time: %s - Command: %s\n", time_str, server_message);

    write(log_fd, log_entry, strlen(log_entry));

    close(log_fd);
}

void process_server_command(const char *server_message, char *response)
{
    snprintf(response, BUFFER_SIZE, "Echoing server message: %s", server_message);
    log_command(server_message);
}

void daemon_init()
{
    pid_t pid = fork();

    if (pid < 0)
    {
        printf("Error: Unable to fork daemon process.\n");
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {
        exit(0);
    }

    if (setsid() < 0)
    {
        printf("Error: Unable to set session ID.\n");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0)
    {
        printf("Error: Unable to fork daemon process.\n");
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {
        exit(0);
    }

    umask(0);

    if (chdir("/") < 0)
    {
        printf("Error: Unable to change directory to root.\n");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_RDWR);

    openlog("client_daemon", LOG_PID, LOG_DAEMON);

    syslog(LOG_INFO, "Daemon started.");
}

void get_executable_path(char *argv0)
{
    char path_buffer[BUFFER_SIZE];

    ssize_t len = readlink("/proc/self/exe", path_buffer, sizeof(path_buffer) - 1);
    if (len != -1)
    {
        path_buffer[len] = '\0';
        strncpy(executable_path, dirname(path_buffer), BUFFER_SIZE);
    }
    else
    {
        strncpy(executable_path, ".", BUFFER_SIZE);
    }
}

void authenticate_with_server(int client_socket)
{
    char client_info[BUFFER_SIZE];
    char token_file[BUFFER_SIZE];
    char token[37] = {0};

    snprintf(token_file, sizeof(token_file), "%s/%s", executable_path, TOKEN_FILENAME);
    get_username_and_station_name(client_info, sizeof(client_info));

    int fd = open(token_file, O_RDONLY);
    if (fd >= 0)
    {
        read(fd, token, sizeof(token));
        close(fd);
        strcat(client_info, " ");
        strcat(client_info, token);
    }
    //log_command(client_info);
    send(client_socket, client_info, strlen(client_info), 0);
    if (recv(client_socket, token, sizeof(token), 0) > 0)
    {
        log_command("Am primit token");
        fd = open(token_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0)
        {
            write(fd, token, strlen(token));
            close(fd);
        }
        }

}

void connect_to_server(const char *server_ip, int server_port)
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

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        syslog(LOG_ERR, "Error connecting to server");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Connected to server at %s:%d", server_ip, server_port);
    authenticate_with_server(client_socket);
}

void handle_server_messages()
{
    char server_message[BUFFER_SIZE];

    char me[BUFFER_SIZE];
    get_username_and_station_name(me, sizeof(me));

    int bytes_sent = send(client_socket, me, strlen(me), 0);

    if (bytes_sent < 0)
    {
        syslog(LOG_ERR, "Error sending station info to server");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        memset(server_message, 0, BUFFER_SIZE);
        if (recv(client_socket, server_message, sizeof(server_message), 0) < 0)
        {
            syslog(LOG_ERR, "Error receiving message from server");
            break;
        }

        if (strlen(server_message) == 0)
        {
            syslog(LOG_INFO, "Server disconnected.");
            break;
        }

        log_command(server_message);
    }

    close(client_socket);
}

void signal_handler(int sig)
{
    if (sig == SIGTERM)
    {
        syslog(LOG_INFO, "Daemon terminated.");
        close(client_socket);
        exit(0);
    }
}

void get_username_and_station_name(char *station_info, size_t size)
{
    char *username = getenv("USER");
    if (username == NULL)
    {
        struct passwd *pw = getpwuid(getuid());
        if (pw != NULL)
        {
            username = pw->pw_name;
        }
        else
        {
            username = "unknown_user";
        }
    }

    char hostname[BUFFER_SIZE];
    if (gethostname(hostname, sizeof(hostname)) == -1)
    {
        strncpy(hostname, "unknown_station", sizeof(hostname));
    }

    snprintf(station_info, size, "%s:%s", username, hostname);
}