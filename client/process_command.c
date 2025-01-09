#include "client_daemon.h"

bool livestream(const char *server_message, char *response)
{
    if (server_message == NULL || response == NULL)
    {
        strcpy(response, "Error: Invalid parameters");
        return false;
    }

    char filename[256];
    int duration_seconds;

    // Expect input format:  livestream <filename> <duration_in_seconds>
    if (sscanf(server_message, "livestream %255s %d", filename, &duration_seconds) != 2)
    {
        strcpy(response, "Error: Invalid command format. Expected: livestream \"filename\" \"duration\"");
        return false;
    }

    if (duration_seconds <= 0)
    {
        strcpy(response, "Error: Duration must be positive");
        return false;
    }

    // Detect if we’re on Wayland or Xorg by checking $XDG_SESSION_TYPE
    const char *xdg_session_type = getenv("XDG_SESSION_TYPE");
    bool use_wayland = false;
    if (xdg_session_type != NULL && strcmp(xdg_session_type, "wayland") == 0)
    {
        use_wayland = true;
    }

    // Create a pipe to capture ffmpeg's stdout
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        snprintf(response, 256, "Error: Failed to create pipe (%s)", strerror(errno));
        return false;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        snprintf(response, 256, "Error: Failed to fork process (%s)", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0)
    {
        // Child Process
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        // Build the duration as a string
        char duration_str[16];
        snprintf(duration_str, sizeof(duration_str), "%d", duration_seconds);

        /*
         * Depending on the session type, use either:
         *   Xorg:     ffmpeg -f x11grab    -i :0.0
         *   Wayland:  ffmpeg -f pipewire   -i server=pipewire-0
         *
         * Additional ffmpeg flags can be added as needed.
         */

        if (use_wayland)
        {
            // Wayland capture via PipeWire
            execlp("ffmpeg", "ffmpeg",
                   "-f", "pipewire",
                   "-i", "server=pipewire-0",
                   "-t", duration_str,
                   "-c:v", "libx264",
                   "-preset", "ultrafast",
                   filename,
                   NULL);
        }
        else
        {
            // Xorg capture
            // Change ":0.0" if your DISPLAY is different (e.g., :1.0)
            execlp("ffmpeg", "ffmpeg",
                   "-f", "x11grab",
                   "-i", ":0.0",
                   "-t", duration_str,
                   "-c:v", "libx264",
                   "-preset", "ultrafast",
                   filename,
                   NULL);
        }

        // If execlp fails:
        perror("execlp");
        exit(1);
    }
    else
    {
        // Parent Process
        close(pipefd[1]);
        char buffer[1024];
        while (read(pipefd[0], buffer, sizeof(buffer)) > 0)
        {
            // Discard or log ffmpeg's stdout data here
        }
        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && (WEXITSTATUS(status) == 0))
        {
            snprintf(response, 256, "Livestream successfully saved to %s", filename);
            return true;
        }
        else
        {
            strcpy(response, "Error: Livestream failed");
            return false;
        }
    }
}

bool execute_screenshot(const char *server_message, char *response)
{
    if (!server_message || !response)
    {
        syslog(LOG_ERR, "Error: NULL parameters provided");
        return false;
    }
    char *message_copy = strdup(server_message);
    if (!message_copy)
    {
        syslog(LOG_ERR, "Error: Memory allocation failed for message copy");
        return false;
    }
    char filename[256] = {0};
    char *token = strtok(message_copy, " ");
    token = strtok(NULL, " ");
    if (!token)
    {
        syslog(LOG_ERR, "Error: No filename provided in command: %s", server_message);
        free(message_copy);
        snprintf(response, 64, "Error: Invalid screenshot command format");
        return false;
    }
    if (strlen(token) >= sizeof(filename) || strlen(token) == 0)
    {
        syslog(LOG_ERR, "Error: Filename length invalid: %s", token);
        free(message_copy);
        snprintf(response, 64, "Error: Invalid filename length");
        return false;
    }
    if (strstr(token, "..") || strstr(token, "/"))
    {
        syslog(LOG_ERR, "Error: Invalid characters in filename: %s", token);
        free(message_copy);
        snprintf(response, 64, "Error: Invalid filename characters");
        return false;
    }
    strncpy(filename, token, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    char command[512] = {0};
    if (snprintf(command, sizeof(command), "scrot '%s' 2>&1", filename) >= sizeof(command))
    {
        syslog(LOG_ERR, "Error: Command buffer overflow");
        free(message_copy);
        snprintf(response, 64, "Error: Command too long");
        return false;
    }
    FILE *fp = popen(command, "r");
    if (!fp)
    {
        syslog(LOG_ERR, "Error: Failed to execute screenshot command: %s", strerror(errno));
        free(message_copy);
        snprintf(response, 64, "Error: Screenshot command failed");
        return false;
    }

    // Read command output
    char output[256] = {0};
    if (fgets(output, sizeof(output), fp) != NULL)
    {
        // Remove newline if present
        output[strcspn(output, "\n")] = 0;
    }

    int status = pclose(fp);
    free(message_copy);

    if (status != 0)
    {
        syslog(LOG_ERR, "Screenshot failed with status %d: %s", status, output);
        snprintf(response, 64, "Error: Screenshot failed");
        return false;
    }

    snprintf(response, 64, "Screenshot saved to %s", filename);
    return true;
}

void process_server_command(const char *server_message, char *response)
{
    if (!server_message || !response)
    {
        syslog(LOG_ERR, "Invalid input parameters");
        return;
    }

    if (strstr(server_message, "exit") != NULL) {
    syslog(LOG_INFO, "Server requested daemon shutdown.");
    
    const char *exit_message = "exited with success";
    send_data(client_socket, exit_message, strlen(exit_message));
    
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

    if (strstr(server_message, "screenshot") != NULL)
    {
        syslog(LOG_INFO, "Server requested screenshot.");
        if (execute_screenshot(server_message, response))
        {
            return;
        }
    }

    if (strstr(server_message, "livestream") != NULL)
    {
        syslog(LOG_INFO, "Server requested screenshot.");
        if (livestream(server_message, response))
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
