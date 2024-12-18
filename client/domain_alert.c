#define _GNU_SOURCE

#include "client_daemon.h"
#include <time.h>

char blocked_domains[MAX_BLOCKED_DOMAINS][MAX_DOMAIN_LENGTH];
int blocked_count = 0;
pthread_t alert_thread;
int stop_alert_thread = 0;

int load_blocked_domains(const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        perror("Error opening blocked domains file");
        return 0;
    }

    blocked_count = 0;
    char buffer[512];
    ssize_t bytes_read;
    size_t line_start = 0;
    size_t buffer_len = 0;

    while ((bytes_read = read(fd, buffer + buffer_len, sizeof(buffer) - buffer_len - 1)) > 0)
    {
        buffer_len += bytes_read;
        buffer[buffer_len] = '\0';

        char *line;
        while ((line = strchr(buffer + line_start, '\n')) && blocked_count < MAX_BLOCKED_DOMAINS)
        {
            *line = '\0';
            if (strlen(buffer + line_start) > 0)
            {
                strncpy(blocked_domains[blocked_count], buffer + line_start, MAX_DOMAIN_LENGTH - 1);
                blocked_domains[blocked_count][MAX_DOMAIN_LENGTH - 1] = '\0';
                blocked_count++;
            }
            line_start = line - buffer + 1;
        }

        if (line_start < buffer_len)
        {
            memmove(buffer, buffer + line_start, buffer_len - line_start);
            buffer_len -= line_start;
            line_start = 0;
        }
        else
        {
            buffer_len = 0;
        }
    }

    if (bytes_read < 0)
    {
        perror("Error reading blocked domains file");
        close(fd);
        return 0;
    }

    close(fd);
    return blocked_count;
}

int is_domain_blocked(const char *domain)
{
    for (int i = 0; i < blocked_count; i++)
    {
        if (strstr(domain, blocked_domains[i]) != NULL)
        {
            printf("Domain %s is blocked (matches %s)\n", domain, blocked_domains[i]);
            return 1;
        }
    }
    return 0;
}

void *alert_thread_function(void *arg)
{
    if (load_blocked_domains("blocked.txt") == 0)
    {
        write(STDERR_FILENO, "No blocked domains loaded or error reading file\n", 49);
        return NULL;
    }

    const char *regex_pattern = "[a-zA-Z]([a-zA-Z0-9-]+\\.)[a-zA-Z0-9-]+{1,}\\.[a-zA-Z]{2,}";
    regex_t regex;
    if (regcomp(&regex, regex_pattern, REG_EXTENDED))
    {
        write(STDERR_FILENO, "Could not compile regex\n", 24);
        return NULL;
    }

    FILE *tcpdump = popen("sudo tcpdump -i $(ip -o link show | awk -F': ' '{print $2}' | grep -v '^lo$') port 53 -A -n", "r");
    if (!tcpdump)
        return NULL;

    int output_fd = open("domains.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (output_fd < 0)
    {
        pclose(tcpdump);
        return NULL;
    }

    char buffer[128];
    char domain[256];
    while (!stop_alert_thread)
    {
        while (fgets(buffer, sizeof(buffer), tcpdump))
        {
            regmatch_t matches[1];
            if (regexec(&regex, buffer, 1, matches, 0) == 0)
            {
                int start = matches[0].rm_so;
                int end = matches[0].rm_eo;

                strncpy(domain, buffer + start, end - start);
                domain[end - start] = '\0';
                if (is_domain_blocked(domain))
                {
                    time_t current_time;
                    struct tm *time_info;
                    char timestamp[64];

                    time(&current_time);
                    time_info = localtime(&current_time);

                    // YYYY-MM-DD HH:MM:SS
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time_info);

                    char log_entry[512];
                    int log_len = snprintf(log_entry, sizeof(log_entry),
                                           "[%s] Blocked Domain: %s\n",
                                           timestamp, domain);

                    write(output_fd, log_entry, log_len);
                }
            }
        }
        usleep(10000);
    }
    close(output_fd);
    int status = pclose(tcpdump);
    if (status != 0)
    {
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "tcpdump exited with status %d\n", status);
        write(STDERR_FILENO, err_msg, strlen(err_msg));
    }

    regfree(&regex);
    return NULL;
}

void set_alert_active()
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    stop_alert_thread = 0;
    if (pthread_create(&alert_thread, &attr, alert_thread_function, NULL) != 0)
    {
        log_command("failed to create alert thread");
        return;
    }
    pthread_attr_destroy(&attr);
}

void stop_alert()
{
    stop_alert_thread = 1;
}
