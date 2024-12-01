#include "client_daemon.h"

void process_server_command(const char *server_message, char *response)
{
    if (!server_message || !response)
    {
        syslog(LOG_ERR, "Invalid input parameters");
        return;
    }

    if (strstr(server_message, "exit") != NULL)
    {
        syslog(LOG_INFO, "Server requested daemon shutdown.");
        send(client_socket, "exited with success", strlen("exited with success"), 0);
        stop_alert();
        exit(0);
    }

    if (strstr(server_message, "monitor") != NULL)
    {
        syslog(LOG_INFO, "Server requested monitor.");
        if (handle_monitor_command(response) == 0)
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
