#include "client_daemon.h"

int client_socket;
char executable_path[MAX_PATH];

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

void *execute_monitor(void *args)
{
    monitor_args_t *monitor_args = (monitor_args_t *)args;
    run_system_monitor_server(monitor_args->ip, monitor_args->port, monitor_args->duration);
    free(monitor_args);
    return NULL;
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
    { // Parent process
        close(pipe_fd[1]);
        ssize_t bytes_read = read(pipe_fd[0], ip_buffer, buffer_size - 1);
        close(pipe_fd[0]);

        if (bytes_read > 0)
        {
            ip_buffer[12] = '\0'; // Ensure null termination
            return 0;
        }
        return -1;
    }
}

int handle_monitor_command(const char *server_message, char *response)
{
    char *token = strtok((char *)server_message, " ");
    token = strtok(NULL, " ");

    if (token == NULL)
    {
        snprintf(response, BUFFER_SIZE, "monitor command requires duration parameter");
        return -1;
    }

    char ip[IP_BUFFER_SIZE];
    if (get_system_ip(ip, sizeof(ip)) < 0)
    {
        snprintf(response, BUFFER_SIZE, "failed to get system IP");
        return -1;
    }

    monitor_args_t *args = malloc(sizeof(monitor_args_t));
    if (args == NULL)
    {
        snprintf(response, BUFFER_SIZE, "memory allocation failed");
        return -1;
    }

    strncpy(args->ip, ip, IP_BUFFER_SIZE);
    args->port = PORT;
    args->duration = (time_t)atoi(token);

    pthread_t monitor_thread;
    if (pthread_create(&monitor_thread, NULL, execute_monitor, args) != 0)
    {
        free(args);
        snprintf(response, BUFFER_SIZE, "failed to create monitor thread");
        return -1;
    }

    pthread_detach(monitor_thread);

    snprintf(response, BUFFER_SIZE, "monitor %s:%d", ip, PORT);
    return 0;
}

void process_server_command(const char *server_message, char *response)
{
    if (!server_message || !response)
    {
        syslog(LOG_ERR, "Invalid input parameters");
        return;
    }

    if (strcmp(server_message, "exit") == 0)
    {
        syslog(LOG_INFO, "Server requested daemon shutdown.");
        send(client_socket, "exited with success", strlen("exited with success"), 0);
        exit(0);
    }

    if (strstr(server_message, "monitor") != NULL)
    {
        syslog(LOG_INFO, "Server requested monitor.");
        if (handle_monitor_command(server_message, response) == 0)
        {
            return;
        }
    }

    char command[BUFFER_SIZE] = {0};
    char *args[MAX_ARGS] = {NULL};
    char *message_copy = strdup(server_message);

    if (!message_copy)
    {
        snprintf(response, BUFFER_SIZE, "Memory allocation failed");
        syslog(LOG_ERR, "Failed to allocate memory for command parsing");
        return;
    }

    if (parse_command(message_copy, command, args) < 0)
    {
        snprintf(response, BUFFER_SIZE, "Command parsing failed");
        cleanup_args(message_copy);
        return;
    }

    if (strcmp(command, "rm") == 0)
    {
        snprintf(response, BUFFER_SIZE, "rm command blocked");
        syslog(LOG_INFO, "rm command blocked.");
        cleanup_args(message_copy);
        return;
    }

    if (execute_command(command, args, response) < 0)
    {
        syslog(LOG_ERR, "Command execution failed: %s", command);
    }

    cleanup_args(message_copy);
}

int parse_command(char *message_copy, char *command, char **args)
{
    int arg_count = 0;
    char *token = strtok(message_copy, " ");

    while (token != NULL && arg_count < MAX_ARGS - 1)
    {
        if (arg_count == 0)
        {
            strncpy(command, token, BUFFER_SIZE - 1);
            command[BUFFER_SIZE - 1] = '\0';
        }
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    return arg_count;
}

int execute_command(const char *command, char **args, char *response)
{
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
    {
        snprintf(response, BUFFER_SIZE, "Pipe creation failed");
        syslog(LOG_ERR, "Failed to create pipe: %m");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        snprintf(response, BUFFER_SIZE, "Fork failed");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }

    if (pid == 0)
    {
        close(pipe_fd[0]);

        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
        {
            syslog(LOG_ERR, "Failed to redirect stdout: %m");
            exit(EXIT_FAILURE);
        }
        close(pipe_fd[1]);

        execvp(command, args);

        syslog(LOG_ERR, "Failed to execute command %s: %m", command);
        write(STDOUT_FILENO, "Command not found\n", 17);
        exit(EXIT_FAILURE);
    }

    close(pipe_fd[1]);

    int status;
    waitpid(pid, &status, 0);

    ssize_t bytes_read = read(pipe_fd[0], response, BUFFER_SIZE - 1);
    if (bytes_read < 0)
    {
        snprintf(response, BUFFER_SIZE, "Failed to read command output");
        close(pipe_fd[0]);
        return -1;
    }

    response[bytes_read] = '\0';
    close(pipe_fd[0]);

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
    {
        return -1;
    }

    return 0;
}

void cleanup_args(char *message_copy)
{
    if (message_copy)
    {
        free(message_copy);
    }
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
    bool token_exists = false;
    snprintf(token_file, sizeof(token_file), "%s/%s", executable_path, TOKEN_FILENAME);
    get_username_and_station_name(client_info, sizeof(client_info));

    int fd = open(token_file, O_RDONLY);
    if (fd >= 0)
    {
        token_exists = true;
        read(fd, token, sizeof(token));
        close(fd);
        strcat(client_info, " ");
        strcat(client_info, token);
    }
    send(client_socket, client_info, strlen(client_info), 0);
    syslog(LOG_ERR, "SENT CLIENT INFO\n");
    if (!token_exists)
    {
        if (recv(client_socket, token, sizeof(token), 0) > 0)
        {
            log_command("Am primit token");
            fd = open(token_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0)
            {
                write(fd, token, strlen(token));
                close(fd);
            }
            send(client_socket, "client received token.\n", strlen("client2 received token.\n"), 0);
        }
    }
}

bool connect_to_server(const char *server_ip, int server_port)
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
        return false;
    }

    syslog(LOG_INFO, "Connected to server at %s:%d", server_ip, server_port);
    authenticate_with_server(client_socket);
    return true;
}

void handle_server_messages()
{
    char server_message[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    bool first_message = true;

    while (1)
    {
        memset(server_message, 0, BUFFER_SIZE);
        memset(response, 0, BUFFER_SIZE);

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
        if (first_message)
        {
            first_message = false;
        }
        else
        {
            process_server_command(server_message, response);
        }
        log_command(server_message);
        if (send(client_socket, response, strlen(response), 0) < 0)
        {
            syslog(LOG_ERR, "Error sending response to server");
            break;
        }
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