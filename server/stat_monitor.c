#include "serverlib.h"

#define STATS_FILE "client_stats.txt"

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
                    snprintf(auxiliar, 261, "==%s==%s\n", clients[i]->station_info, formatted);
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
