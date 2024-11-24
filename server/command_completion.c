#include "serverlib.h"

void initialize_commands()
{
    commands = malloc(7 * sizeof(char *));
    commands[0] = strdup("clear");
    commands[1] = strdup("exit");
    commands[2] = strdup("list");
    commands[3] = strdup("stats");
    commands[4] = strdup("log");
    commands[5] = strdup("commands");
    commands[6] = NULL;
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

int delete_command(const char *command_to_delete)
{
    if (command_to_delete == NULL || commands == NULL)
    {
        return 0; // Nu avem ce șterge
    }

    size_t cmd_count = 0;
    while (commands[cmd_count] != NULL)
    {
        cmd_count++; // Numărăm câte comenzi există
    }

    if (cmd_count == 0)
    {
        return 0; // Nu există comenzi de șters
    }

    size_t read_idx, write_idx;
    int found = 0;

    for (read_idx = 0, write_idx = 0; read_idx < cmd_count; read_idx++)
    {
        if (strcmp(commands[read_idx], command_to_delete) == 0)
        {
            free(commands[read_idx]); // Eliberăm memoria pentru comanda găsită
            found = 1;
        }
        else
        {
            // Mutăm elementele valide spre început
            commands[write_idx] = commands[read_idx];
            write_idx++;
        }
    }

    if (!found)
    {
        return 0; // Nu s-a găsit comanda de șters
    }

    // Ultimul element trebuie să fie NULL
    commands[write_idx] = NULL;

    // Redimensionăm array-ul
    char **temp = realloc(commands, (write_idx + 1) * sizeof(char *));
    if (temp != NULL)
    {
        commands = temp;
    }

    return 1; // Comanda a fost ștearsă cu succes
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
