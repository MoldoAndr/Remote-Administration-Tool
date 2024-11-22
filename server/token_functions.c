#include "serverlib.h"

#define TOKEN_FILE "client_tokens.txt"

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
