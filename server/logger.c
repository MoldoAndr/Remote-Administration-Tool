#include "serverlib.h"

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
