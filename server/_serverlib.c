#include "serverlib.h"

#define TOKEN_FILE "client_tokens.txt"
#define STATS_FILE "client_stats.txt"

pthread_mutex_t client_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_count = 0;
struct client_info *clients[MAX_CLIENTS];
char *log_folder = "client_logs";

void initialize_commands()
{
    commands = malloc(4 * sizeof(char *));
    commands[0] = strdup("clear");
    commands[1] = strdup("exit");
    commands[2] = strdup("list");
    commands[3] = strdup("stats");
    commands[3] = NULL;
}

void add_command(const char *new_command)
{
    size_t count = 0;
    while (commands[count] != NULL)
    {
        count++;
    }

    commands = realloc(commands, (count + 2) * sizeof(char *));

    commands[count] = strdup(new_command);
    commands[count + 1] = NULL;
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

int delete_command(const char *command_to_delete)
{
    if (command_to_delete == NULL || commands == NULL)
    {
        return 0;
    }

    size_t cmd_count = 0;
    while (commands[cmd_count] != NULL)
    {
        cmd_count++;
    }

    if (cmd_count == 0)
    {
        return 0;
    }

    size_t read_idx, write_idx;
    int found = 0;

    for (read_idx = 0, write_idx = 0; read_idx < cmd_count; read_idx++)
    {
        if (strcmp(commands[read_idx], command_to_delete) == 0)
        {
            free(commands[read_idx]);
            found = 1;
            continue;
        }

        if (read_idx != write_idx)
        {
            commands[write_idx] = commands[read_idx];
        }
        write_idx++;
    }

    if (!found)
    {
        return 0;
    }

    commands[write_idx] = NULL;
    size_t new_size = (write_idx + 1) * sizeof(char *);

    if (write_idx > 0)
    {
        char **temp = realloc(commands, new_size);
        if (temp == NULL)
        {
            return 1;
        }
        commands = temp;
    }
    else
    {
        free(commands);
        commands = malloc(sizeof(char *));
        if (commands == NULL)
        {
            return 0;
        }
        commands[0] = NULL;
    }

    return 1;
}

void free_commands()
{
    size_t i = 0;
    while (commands[i] != NULL)
    {
        free(commands[i]);
        i++;
    }
    free(commands);
}

void generate_token(char *token)
{
    uuid_t binuuid;
    uuid_generate(binuuid);
    uuid_unparse(binuuid, token);
}

void store_token(char *station_name, char *token)
{
    if (token == NULL)
    {
        fprintf(stderr, "Error: token is NULL\n");
        return;
    }

    if (strlen(token) > 255)
    {
        fprintf(stderr, "Error: token is too long\n");
        return;
    }

    int fd = open(TOKEN_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        perror("Error opening token file");
        return;
    }

    char token2[256];
    strcpy(token2, station_name);
    strcat(token2, " ");
    strcat(token2, token);
    strcat(token2, "\n");

    ssize_t bytes_written = write(fd, token2, strlen(token2));
    if (bytes_written < 0)
    {
        perror("Error writing to token file");
    }

    close(fd);
}

bool validate_token(const char *client_info, const char *received_token)
{
    int fd = open(TOKEN_FILE, O_RDONLY);
    if (fd < 0)
    {
        return false;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
    close(fd);

    if (bytes_read <= 0)
    {
        return false;
    }
    buffer[bytes_read] = '\0';
    char *line = strtok(buffer, "\n");

    while (line != NULL)
    {
        char stored_client[BUFFER_SIZE], stored_token[BUFFER_SIZE];
        sscanf(line, "%s %s", stored_client, stored_token);

        if (strcmp(stored_client, client_info) == 0 && strcmp(stored_token, received_token) == 0)
        {
            return true;
        }
        line = strtok(NULL, "\n");
    }
    return false;
}

void setup_server(struct sockaddr_in *server_addr, int *socket_desc, char *server_address)
{
    struct stat st = {0};
    *socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    initialize_commands();

    if (stat(log_folder, &st) == -1)
    {
        mkdir(log_folder, 0700);
    }

    if (*socket_desc < 0)
    {
        perror("Error while creating socket");
        exit(EXIT_FAILURE);
    }

    write(1, "Socket created successfully\n", 29);

    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(12345);
    server_addr->sin_addr.s_addr = inet_addr(server_address);

    if (bind(*socket_desc, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        perror("Couldn't bind to the port");
        exit(EXIT_FAILURE);
    }

    printf("Done with binding\n");

    if (listen(*socket_desc, MAX_CLIENTS) < 0)
    {
        perror("Error while listening");
        exit(EXIT_FAILURE);
    }

    pthread_t thread_id;

    if (pthread_create(&thread_id, NULL, background_writer, NULL) < 0)
    {
        printf("Could not create thread\n");
        return;
    }

    printf("Listening for incoming connections.....\n");
}

void log_command(const char *station_name, const char *command, const char *output)
{
    char log_filename[BUFFER_SIZE];

    snprintf(log_filename, sizeof(log_filename), "%s/%s.log", log_folder, station_name);

    int log_fd = open(log_filename, O_WRONLY | O_APPEND | O_CREAT, 0644);

    if (log_fd < 0)
    {
        write(1, "Could not open log file\n", 24);
        return;
    }

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    char time_buffer[BUFFER_SIZE];

    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", local_time);

    char log_entry[BUFFER_SIZE];
    int log_entry_len = snprintf(log_entry, sizeof(log_entry), "Date/Time:%s\nOutput:\n%s", time_buffer, output);

    if (log_entry_len > 0 || log_entry_len < BUFFER_SIZE)
    {
        write(log_fd, log_entry, log_entry_len);
    }

    close(log_fd);
}

void accept_clients(int socket_desc, struct sockaddr_in *server_addr)
{
    socklen_t client_size;
    pthread_t thread_id;

    while ("pso")
    {
        struct client_info *client = malloc(sizeof(struct client_info));
        client_size = sizeof(client->address);
        client->socket = accept(socket_desc, (struct sockaddr *)&client->address, &client_size);

        if (client->socket < 0)
        {
            printf("Can't accept\n");
            free(client);
            continue;
        }

        pthread_mutex_lock(&client_count_mutex);
        client->id = ++client_count;
        pthread_mutex_unlock(&client_count_mutex);

        char client_string[64];
        snprintf(client_string, 64, "client%d:", client->id);
        add_command(client_string);

        printf("client%d connected at IP: %s and port: %i\n",
               client->id, inet_ntoa(client->address.sin_addr), ntohs(client->address.sin_port));

        pthread_mutex_lock(&clients_mutex);
        clients[client->id - 1] = client;
        pthread_mutex_unlock(&clients_mutex);

        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) < 0)
        {
            printf("Could not create thread\n");
            free(client);
            continue;
        }

        pthread_detach(thread_id);
    }
}

void send_to_client(int client_id, const char *message)
{
    pthread_mutex_lock(&clients_mutex);

    if (client_id > 0 && client_id <= MAX_CLIENTS && clients[client_id - 1] != NULL)
    {
        struct client_info *client = clients[client_id - 1];

        if (send(client->socket, message, strlen(message), 0) < 0)
        {
            printf("Failed to send message to client%d\n", client_id);
        }
    }
    else
    {
        printf("Client%d not found or disconnected\n", client_id);
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg)
{
    struct client_info *client = (struct client_info *)arg;
    char server_message[BUFFER_SIZE], client_message[BUFFER_SIZE];
    char client_id_str[20];

    snprintf(client_id_str, sizeof(client_id_str), "client%d", client->id);

    if (recv(client->socket, client_message, sizeof(client_message), 0) <= 0)
    {
        printf("Client disconnected before initial message, %s\n", client_id_str);
        cleanup_client(client);
        return NULL;
    }

    printf("Received station_info : %s\n", client_message);

    if (!strchr(client_message, ' '))
    {
        char token[37];
        generate_token(token);
        send_to_client(client->id, token);
        store_token(client_message, token);
    }
    else
    {
        char *station_name = strtok(client_message, " ");
        char *received_token = strtok(NULL, " ");
        if (!validate_token(station_name, received_token))
        {
            printf("Invalid token received from %s\n", client_id_str);
            cleanup_client(client);
            return NULL;
        }
        else
        {
            printf("Token validated for %s\n", client_id_str);
        }
    }
    strcpy(client->station_info, client_message);

    send_to_client(client->id, "Server received your station info.");

    while (1)
    {
        memset(client_message, 0, sizeof(client_message));
        memset(server_message, 0, sizeof(server_message));

        ssize_t recv_status = recv(client->socket, client_message, sizeof(client_message), 0);

        if (recv_status < 0)
        {
            perror("Error receiving from client");
            break;
        }
        else if (recv_status == 0)
        {
            printf("%s disconnected\n", client_id_str);
            break;
        }

        printf("Msg from %s:\n%s\n", client_id_str, client_message);
        log_command(client->station_info, "", client_message);
    }

    cleanup_client(client);
    return NULL;
}

void cleanup_client(struct client_info *client)
{
    if (client == NULL)
        return;
    pthread_mutex_lock(&clients_mutex);

    if (client->id > 0 && client->id <= MAX_CLIENTS)
    {
        char client_delete[64];
        snprintf(client_delete, sizeof(client_delete), "client%d:", client->id);
        delete_command(client_delete);
        clients[client->id - 1] = NULL;
    }

    pthread_mutex_unlock(&clients_mutex);

    if (client->socket >= 0)
    {
        close(client->socket);
    }
    free(client);
    client = NULL;
}

void send_to_client_list(const char *client_list, const char *command)
{
    char *list_copy = strdup(client_list);
    char *token = strtok(list_copy, ",");

    while (token != NULL)
    {
        char *dash = strchr(token, '-');
        if (dash)
        {
            *dash = '\0';
            int start = atoi(token);
            int end = atoi(dash + 1);

            if (start > 0 && end > 0 && start <= MAX_CLIENTS && end <= MAX_CLIENTS)
            {
                for (int i = start; i <= end; i++)
                {
                    send_to_client(i, command);
                    printf("Command sent to client%d: %s\n", i, command);
                }
            }
            else
            {
                printf("Invalid client range: %s-%s\n", token, dash + 1);
            }
        }
        else
        {
            int client_id = atoi(token);
            if (client_id > 0 && client_id <= MAX_CLIENTS)
            {
                send_to_client(client_id, command);
                printf("Command sent to client%d: %s\n", client_id, command);
            }
            else
            {
                printf("Invalid client ID: %s\n", token);
            }
        }
        token = strtok(NULL, ",");
    }

    free(list_copy);
}

char *command_generator(const char *text, int state)
{
    static int list_index, len;
    const char *name;

    if (!state)
    {
        list_index = 0;
        len = strlen(text);
    }

    while ((name = commands[list_index]))
    {
        list_index++;
        if (strncmp(name, text, len) == 0)
        {
            return strdup(name);
        }
    }

    return NULL;
}

char **custom_completion(const char *text, int start, int end)
{
    char **matches = NULL;

    if (start == 0)
    {
        matches = rl_completion_matches(text, command_generator);
    }

    return matches;
}

void update_active_clients()
{
    num_active_clients = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != NULL)
        {
            active_client_numbers[num_active_clients++] = i;
        }
    }
}

void info()
{
    printf("\n\n");
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    int terminalWidth = w.ws_col;

    char first[] = "Enter command (clientX:command or clientX,Y,Z:command or clientX-Y:command)\n";
    char second[] = "Use TAB for autocomplete, 'clear' to clear console, or 'exit' to quit or 'list' to list connected clients or stats:\n";

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
    char RAT_LOGO[][256] = {"  ░▒▓███████▓▒░  ░▒▓██████▓▒░ ▒▓███████▓▒░\n",
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

int extract_numbers(const char *str, float *numbers)
{
    int count = 0;
    char *ptr = (char *)str;

    while (*ptr)
    {
        while (*ptr && !isdigit(*ptr) && *ptr != '-' && *ptr != '.')
        {
            ptr++;
        }

        if (*ptr)
        {
            char *endptr;
            float num = strtof(ptr, &endptr);
            if (ptr != endptr)
            {
                numbers[count++] = num;
                ptr = endptr;
            }
            else
            {
                ptr++;
            }
        }
    }
    return count;
}

float *get_metrics(const char *url)
{
    int pipefd[2];
    pid_t pid;
    static float numbers[MAX_NUMBERS];
    char buffer[BUFFER_SIZE];

    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return NULL;
    }

    pid = fork();

    if (pid == -1)
    {
        perror("fork");
        return NULL;
    }

    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execlp("curl", "curl", "-s", url, NULL);
        perror("execlp");
        exit(1);
    }
    else
    {
        close(pipefd[1]);
        ssize_t bytes_read = read(pipefd[0], buffer, BUFFER_SIZE - 1);
        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';
            int count = extract_numbers(buffer, numbers);
            numbers[count] = -1;
        }

        close(pipefd[0]);
        wait(NULL);
    }

    return numbers;
}

char *format_data(float *data)
{
    if (data == NULL)
    {
        fprintf(stderr, "Data is NULL\n");
        return NULL;
    }
    char *formatted_data = malloc(256);
    if (formatted_data == NULL)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    snprintf(formatted_data, 256,
             "\n  CPU:%.2f\n  RAM:%.2f\n  DiskRead:%.2f\n  DiskWrite:%.2f\n  NetRx:%.2f\n  NetTx:%.2f\n  DiskUsage:%.2f\n",
             data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    return formatted_data;
}

char *process_clients_statistics()
{
    float *data;
    char url[256];
    char *total_data = (char *)malloc(MAX_CLIENTS * 256);
    char auxiliar[256];

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i] != NULL)
        {
            snprintf(url, sizeof(url), "http://%s:52577/data", inet_ntoa(clients[i]->address.sin_addr));
            data = get_metrics(url);

            if (data != NULL)
            {
                char *formatted = format_data(data);
                if (formatted != NULL)
                {
                    snprintf(auxiliar, 256, "==%s==%s\n", clients[i]->station_info, formatted);
                    if (!i)
                        strcpy(total_data, auxiliar);
                    else
                        strcat(total_data, auxiliar);
                }
            }
            else
            {
                fprintf(stderr, "Failed to retrieve metrics for client: %s\n", clients[i]->station_info);
                return NULL;
            }
            memset(url, 0, sizeof(url));
        }
    }

    total_data[strlen(total_data) - 1] = '\0';
    pthread_mutex_unlock(&clients_mutex);
    return total_data;
}

void write_statistics()
{
    int fd = open(STATS_FILE, O_CREAT | O_WRONLY, 0666);

    if (fd == -1)
    {
        write(STDERR_FILENO, "ERROR OPENING THE STATS FILE\n", 30);
        return;
    }

    char *processed_output = process_clients_statistics();
    usleep(10000);

    int written = write(fd, processed_output, strlen(processed_output));

    if (written < 0)
    {
        write(STDERR_FILENO, "ERROR WRITTING TO THE STATS FILE\n", 34);
    }

    free(processed_output);
    close(fd);

    return;
}

void *background_writer()
{
    while (true)
    {
        sleep(1);
        write_statistics();
    }
    return NULL;
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
        printf("███▓▒░ ");
        usleep(10000);

        input = readline(NULL);

        if (input == NULL)
        {
            printf("Error reading input. Exiting.\n");
            break;
        }

        if (input[0] != '\0')
        {
            add_history(input);
        }

        if (strstr(input, "exit") == input)
        {
            printf("Exiting terminal input handler.\n");
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i] != NULL)
                {
                    close(clients[i]->socket);
                    free(clients[i]);
                }
            }
            free_commands();
            free(input);
            rl_clear_history();
            exit(EXIT_SUCCESS);
        }

        if (strstr(input, "clear") == input)
        {
            system("clear");
            info();
            free(input);
            continue;
        }

        if (strstr(input, "list") == input)
        {
            list_clients();
            free(input);
            continue;
        }

        if (strstr(input, "stats") == input)
        {
            char *processed = process_clients_statistics();
            printf("%s", processed);
            if (processed)
                free(processed);
            free(input);
            continue;
        }

        if (strncmp(input, "client", 6) != 0)
        {
            info();
            free(input);
            continue;
        }

        char *colon = strchr(input, ':');
        if (colon == NULL)
        {
            info();
            free(input);
            continue;
        }

        *colon = '\0';
        char *client_list = input + 6;
        char *command = colon + 1;

        if (strlen(command) == 0)
        {
            printf("Command cannot be empty.\n");
            free(input);
            continue;
        }

        if (strlen(client_list) == 0)
        {
            printf("Client list cannot be empty.\n");
            free(input);
            continue;
        }

        send_to_client_list(client_list, command);
        free(input);

        usleep(10000);
    }
}
