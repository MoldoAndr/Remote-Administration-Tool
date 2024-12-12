#include "serverlib.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define STATS_FILE "client_stats.txt"

struct HttpRequestData
{
    const char *url;
    char *response;
    int done;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

int extract_numbers(const char *str, float *numbers)
{
    int count = 0;
    const char *ptr = str;

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
                if (count < MAX_NUMBERS)
                {
                    numbers[count++] = num;
                }
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

void *curl_thread(void *arg)
{
    struct HttpRequestData *req = (struct HttpRequestData *)arg;
    FILE *fp;
    char command[BUFFER_SIZE];

    snprintf(command, sizeof(command), "curl -s %s", req->url);

    fp = popen(command, "r");
    if (fp == NULL)
    {
        pthread_mutex_lock(&req->mutex);
        req->done = -1;
        pthread_cond_signal(&req->cond);
        pthread_mutex_unlock(&req->mutex);
        return NULL;
    }

    req->response = malloc(BUFFER_SIZE);
    if (req->response == NULL)
    {
        pclose(fp);
        pthread_mutex_lock(&req->mutex);
        req->done = -1;
        pthread_cond_signal(&req->cond);
        pthread_mutex_unlock(&req->mutex);
        return NULL;
    }

    size_t bytes_read = fread(req->response, 1, BUFFER_SIZE - 1, fp);
    req->response[bytes_read] = '\0';

    pclose(fp);

    pthread_mutex_lock(&req->mutex);
    req->done = 1;
    pthread_cond_signal(&req->cond);
    pthread_mutex_unlock(&req->mutex);

    return NULL;
}

float *get_metrics(const char *url)
{
    static float numbers[MAX_NUMBERS];
    pthread_t thread;
    struct HttpRequestData req = {
        .url = url,
        .response = NULL,
        .done = 0};

    pthread_mutex_init(&req.mutex, NULL);
    pthread_cond_init(&req.cond, NULL);

    if (pthread_create(&thread, NULL, curl_thread, &req) != 0)
    {
        pthread_mutex_destroy(&req.mutex);
        pthread_cond_destroy(&req.cond);
        return NULL;
    }

    pthread_mutex_lock(&req.mutex);
    while (!req.done)
    {
        pthread_cond_wait(&req.cond, &req.mutex);
    }
    pthread_mutex_unlock(&req.mutex);

    if (req.done > 0 && req.response != NULL)
    {
        int count = extract_numbers(req.response, numbers);
        if (count > 0)
        {
            numbers[count] = -1;
        }
        free(req.response);
    }

    pthread_join(thread, NULL);
    pthread_mutex_destroy(&req.mutex);
    pthread_cond_destroy(&req.cond);

    return (req.done > 0) ? numbers : NULL;
}

char *format_data(float *data)
{
    if (!data)
    {
        return NULL;
    }

    char *formatted_data = malloc(512);
    if (!formatted_data)
    {
        return NULL;
    }

    if (snprintf(formatted_data, 512,
                 "CPU:%.2f\nRAM:%.2f\nDiskRead:%.2f\nDiskWrite:%.2f\nNetRx:%.2f\nNetTx:%.2f\nDiskUsage:%.2f\n",
                 data[0], data[1], data[2], data[3], data[4], data[5], data[6]) >= 512)
    {
        free(formatted_data);
        return NULL;
    }

    return formatted_data;
}

char *process_clients_statistics()
{
    sleep(1);
    char url[256];
    char *total_data = malloc(MAX_CLIENTS * 256);
    if (!total_data)
    {
        return NULL;
    }
    total_data[0] = '\0';

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i] && client_exists(i + 1))
        {
            char ip_str[INET_ADDRSTRLEN];
            if (!inet_ntop(AF_INET, &clients[i]->address.sin_addr, ip_str, sizeof(ip_str)))
            {
                continue;
            }

            snprintf(url, sizeof(url), "http://%s:52577/data", ip_str);
            float *data = get_metrics(url);
            if (!data)
            {
                continue;
            }

            char *formatted = format_data(data);
            if (!formatted)
            {
                continue;
            }

            size_t current_len = strlen(total_data);
            size_t formatted_len = strlen(formatted);

            if (current_len + formatted_len + 2 < MAX_CLIENTS * 256)
            {
                strncat(total_data, formatted, formatted_len);
                strncat(total_data, "\n", 1);
            }

            free(formatted);
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    return total_data;
}

void write_statistics()
{
    char *processed_output = process_clients_statistics();
    if (!processed_output)
    {
        return;
    }

    int fd = open(STATS_FILE, O_CREAT | O_RDWR, 0644);
    if (fd == -1)
    {
        free(processed_output);
        return;
    }

    size_t length = strlen(processed_output);
    if (ftruncate(fd, length) == -1)
    {
        close(fd);
        free(processed_output);
        return;
    }

    char *mapped = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED)
    {
        close(fd);
        free(processed_output);
        return;
    }

    memcpy(mapped, processed_output, length);
    munmap(mapped, length);

    close(fd);
    free(processed_output);
}

void *background_writer()
{
    while (true)
    {
        sleep(5);
        write_statistics();
    }
    return NULL;
}
