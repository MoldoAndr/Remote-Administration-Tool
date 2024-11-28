#include "serverlib.h"

#define STATS_FILE "client_stats.txt"

struct HttpRequestData {
    const char *url;
    char *response;
    int done;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

int extract_numbers(const char *str, float *numbers) {
    int count = 0;
    const char *ptr = str;

    while (*ptr) {
        while (*ptr && !isdigit(*ptr) && *ptr != '-' && *ptr != '.') {
            ptr++;
        }

        if (*ptr) {
            char *endptr;
            float num = strtof(ptr, &endptr);
            if (ptr != endptr) {
                numbers[count++] = num;
                ptr = endptr;
            } else {
                ptr++;
            }
        }
    }
    return count;
}

void *curl_thread(void *arg) {
    struct HttpRequestData *req = (struct HttpRequestData *)arg;
    FILE *fp;
    char command[BUFFER_SIZE];

    snprintf(command, BUFFER_SIZE, "curl -s %s", req->url);

    fp = popen(command, "r");
    if (fp == NULL) {
        pthread_mutex_lock(&req->mutex);
        req->done = -1;
        pthread_cond_signal(&req->cond);
        pthread_mutex_unlock(&req->mutex);
        return NULL;
    }

    req->response = malloc(BUFFER_SIZE);
    if (req->response == NULL) {
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

float *get_metrics(const char *url) {
    static float numbers[MAX_NUMBERS];
    pthread_t thread;
    struct HttpRequestData req = {
        .url = url,
        .response = NULL,
        .done = 0};

    pthread_mutex_init(&req.mutex, NULL);
    pthread_cond_init(&req.cond, NULL);

    if (pthread_create(&thread, NULL, curl_thread, &req) != 0) {
        pthread_mutex_destroy(&req.mutex);
        pthread_cond_destroy(&req.cond);
        return NULL;
    }

    pthread_mutex_lock(&req.mutex);
    while (!req.done) {
        pthread_cond_wait(&req.cond, &req.mutex);
    }
    pthread_mutex_unlock(&req.mutex);

    if (req.done > 0 && req.response != NULL) {
        int count = extract_numbers(req.response, numbers);
        if (count == 0) {
            free(req.response);
            req.response = NULL;
        } else {
            numbers[count] = -1;
        }
        free(req.response);
    }

    pthread_join(thread, NULL);
    pthread_mutex_destroy(&req.mutex);
    pthread_cond_destroy(&req.cond);

    return (req.done > 0 && req.response != NULL) ? numbers : NULL;
}

char *format_data(float *data) {
    if (data == NULL) {
        fprintf(stderr, "Data is NULL\n");
        return NULL;
    }

    char *formatted_data = malloc(256);
    if (formatted_data == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    snprintf(formatted_data, 256,
             "\n  CPU:%.2f\n  RAM:%.2f\n  DiskRead:%.2f\n  DiskWrite:%.2f\n  NetRx:%.2f\n  NetTx:%.2f\n  DiskUsage:%.2f\n",
             data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    return formatted_data;
}

char *process_clients_statistics() {
    float *data;
    char url[256];
    char *total_data = malloc(MAX_CLIENTS * 256);

    if (!total_data) {
        perror("malloc failed");
        return NULL;
    }
    total_data[0] = '\0';

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++) {
        if (clients[i] != NULL && client_exists(i + 1)) {
            snprintf(url, sizeof(url), "http://%s:52577/data", inet_ntoa(clients[i]->address.sin_addr));
            data = get_metrics(url);

            if (data != NULL) {
                char *formatted = format_data(data);
                if (formatted != NULL) {
                    char auxiliar[280];
                    snprintf(auxiliar, sizeof(auxiliar), "==%s==%s\n", clients[i]->station_info, formatted);
                    if (strlen(total_data) + strlen(auxiliar) + 1 < MAX_CLIENTS * 256) {
                        strcat(total_data, auxiliar);
                    } else {
                        fprintf(stderr, "Buffer overflow detected in total_data\n");
                        free(formatted);
                        free(total_data);
                        total_data = NULL;
                        pthread_mutex_unlock(&clients_mutex);
                        return NULL;
                    }
                    free(formatted);
                }
            } else {
                fprintf(stderr, "Failed to retrieve metrics for client: %s\n", clients[i]->station_info);
                free(total_data);
                total_data = NULL;
                pthread_mutex_unlock(&clients_mutex);
                return NULL;
            }
        }
    }

    size_t len = strlen(total_data);
    if (len > 0) {
        total_data[len] = '\0';
    }

    pthread_mutex_unlock(&clients_mutex);
    return total_data;
}

void write_statistics() {
    char *processed_output = NULL;
    int fd = -1;
    struct flock fl = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0,
    };

    processed_output = process_clients_statistics();
    if (!processed_output) {
        fprintf(stderr, "Failed to process statistics\n");
        return;
    }

    fd = open(STATS_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        fprintf(stderr, "Failed to open stats file: %s\n", strerror(errno));
        free(processed_output);
        return;
    }

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        fprintf(stderr, "Failed to lock file: %s\n", strerror(errno));
        goto cleanup;
    }

    size_t total_len = strlen(processed_output);
    size_t bytes_written = 0;

    while (bytes_written < total_len) {
        ssize_t written = write(fd, processed_output + bytes_written, total_len - bytes_written);
        if (written == -1) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "Write failed: %s\n", strerror(errno));
            goto cleanup;
        }
        bytes_written += written;
    }

cleanup:
    fl.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &fl);

    if (fd != -1)
        close(fd);
    free(processed_output);
}

void *background_writer() {
    while (true) {
        sleep(1);
        write_statistics();
    }
    return NULL;
}