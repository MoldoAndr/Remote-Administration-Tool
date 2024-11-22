#include "client_daemon.h"

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

int get_system_ip(char *ip_buffer, size_t buffer_size)
{
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0)
    {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    if (pid == 0)
    {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        system("hostname --all-ip-addresses");
        close(pipe_fd[1]);
        exit(0);
    }
    else
    {
        close(pipe_fd[1]);
        ssize_t bytes_read = read(pipe_fd[0], ip_buffer, buffer_size - 1);
        close(pipe_fd[0]);

        if (bytes_read > 0)
        {
            ip_buffer[bytes_read - 1] = '\0';
            return 0;
        }
        return -1;
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
