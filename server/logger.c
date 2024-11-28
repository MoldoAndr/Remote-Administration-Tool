#include "serverlib.h"

char *clean_filename(const char *input)
{
    static char clean[BUFFER_SIZE];
    int i = 0;

    while (input[i] != '\0' && i < BUFFER_SIZE - 1)
    {
        if (input[i] == ' ' || input[i] == '/' || input[i] == '_')
        {
            break;
        }
        clean[i] = input[i];
        i++;
    }

    clean[i] = '\0';
    return clean;
}

void log_command(const char *station_name, const char *command, const char *output)
{
    char log_filename[BUFFER_SIZE];

    snprintf(log_filename, sizeof(log_filename), "%s/%s.log", log_folder, clean_filename(station_name));

    int log_fd = open(log_filename, O_WRONLY | O_APPEND | O_CREAT, 0644);

    // printf("\n%s\n", log_filename);

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
    int log_entry_len = snprintf(log_entry, sizeof(log_entry), "Date/Time: %s\nCommand: %s\nOutput:\n%s\n", time_buffer, command, output);

    if (log_entry_len > 0 && log_entry_len < BUFFER_SIZE)
    {
        write(log_fd, log_entry, log_entry_len);
    }

    close(log_fd);
}