#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <regex.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_BLOCKED_DOMAINS 1000
#define MAX_DOMAIN_LENGTH 256
#define BUFFER_SIZE 4096

char blocked_domains[MAX_BLOCKED_DOMAINS][MAX_DOMAIN_LENGTH];
int blocked_count = 0;

int load_blocked_domains(const char *filename)
{
    printf("Loading blocked domains from file: %s\n", filename);

    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        perror("Error opening blocked domains file");
        return 0;
    }

    blocked_count = 0;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    size_t line_start = 0;
    size_t buffer_len = 0;

    while ((bytes_read = read(fd, buffer + buffer_len, sizeof(buffer) - buffer_len - 1)) > 0)
    {
        printf("Read %ld bytes from file\n", bytes_read);
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
                printf("Loaded blocked domain: %s\n", blocked_domains[blocked_count]);
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

    printf("Finished loading blocked domains. Total: %d\n", blocked_count);
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

int main()
{
    printf("Program started\n");

    if (load_blocked_domains("blocked.txt") == 0)
    {
        write(STDERR_FILENO, "No blocked domains loaded or error reading file\n", 49);
    }

    const char *regex_pattern = "[a-zA-Z]([a-zA-Z0-9-]+\\.)[a-zA-Z0-9-]+{1,}\\.[a-zA-Z]{2,}";
    regex_t regex;
    if (regcomp(&regex, regex_pattern, REG_EXTENDED))
    {
        write(STDERR_FILENO, "Could not compile regex\n", 24);
        return 1;
    }

    printf("Regex compiled successfully\n");

    FILE *tcpdump = popen("sudo tcpdump -i wlp1s0 port 53 -A -n", "r");
    if (!tcpdump)
    {
        perror("Error running tcpdump");
        return 1;
    }

    int output_fd = open("domains.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (output_fd < 0)
    {
        perror("Error opening output file");
        pclose(tcpdump);
        return 1;
    }

    printf("Listening to DNS traffic...\n");

    char buffer[BUFFER_SIZE];
    char domain[256];

    while (1)
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

                printf("Extracted domain: %s\n", domain);

                if (is_domain_blocked(domain))
                {
                    printf("Logging blocked domain: %s\n", domain);
                    write(output_fd, domain, strlen(domain));
                    write(output_fd, "\n", 1);
                }
            }
            else
            {
                printf("No match found in packet\n");
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
    printf("Program terminated\n");
    return 0;
}