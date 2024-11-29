#include "serverlib.h"

char **custom_completion(const char *text, int start, int end)
{
    char **matches = NULL;

    if (start == 0)
    {
        matches = rl_completion_matches(text, command_generator);
    }

    return matches;
}


unsigned int hash_command(const char *str)
{
    if (strncmp(str, "client", 6) == 0)
    {
        int client_num = atoi(str + 6);
        if (client_num >= 1 && client_num <= 256)
        {
            return client_num + 5;
        }
    }

    switch (str[0])
    {
    case 'c':
        if (strcmp(str, "clear") == 0)
            return 0;
        if (strcmp(str, "commands") == 0)
            return 1;
        break;
    case 'e':
        if (strcmp(str, "exit") == 0)
            return 2;
        break;
    case 's':
        if (strcmp(str, "stats") == 0)
            return 3;
        break;
    case 'l':
        if (strcmp(str, "log") == 0)
            return 4;
        if (strcmp(str, "list") == 0)
            return 5;
        break;
    }
    return 0;
}

void initialize_commands()
{
    memset(commands, 0, sizeof(commands));

    strcpy(commands[0], "clear");
    strcpy(commands[1], "commands");
    strcpy(commands[2], "exit");
    strcpy(commands[3], "stats");
    strcpy(commands[4], "log");
    strcpy(commands[5], "list");
}

char *command_generator(const char *text, int state)
{
    static int i;
    if (!state)
        i = 0;

    while (i < MAX_COMMANDS)
    {
        if (commands[i][0] != '\0' && strncmp(commands[i], text, strlen(text)) == 0)
        {
            return strdup(commands[i++]);
        }
        i++;
    }
    return NULL;
}

void add_command(const char *new_command)
{
    unsigned int hash = hash_command(new_command);
    
    if (commands[hash][0] == '\0')
    {
        strncpy(commands[hash], new_command, MAX_COMMAND_LENGTH - 1);
        commands[hash][MAX_COMMAND_LENGTH - 1] = '\0';
    }
    else
    {
        printf("Coliziune la adăugarea comenzii: %s\n", new_command);
    }
}

int delete_command(const char *command_to_delete)
{
    unsigned int hash = hash_command(command_to_delete);
    
    if (strcmp(commands[hash], command_to_delete) == 0)
    {
        memset(commands[hash], 0, MAX_COMMAND_LENGTH);
        return 1;
    }
    
    return 0;
}

