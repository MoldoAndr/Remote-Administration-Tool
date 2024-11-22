#include "serverlib.h"

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

        if (strstr(input, "clear") == input || strlen(input) == 0)
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
            char* processed = process_clients_statistics();
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