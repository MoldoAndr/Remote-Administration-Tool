#include "client_daemon.h"

static bool active = false;

void *execute_monitor(void *args)
{
    monitor_args_t *monitor_args = (monitor_args_t *)args;
    run_system_monitor_server(monitor_args->ip, monitor_args->port, monitor_args->duration);
    free(monitor_args);
    return NULL;
}

int handle_monitor_command(char *response)
{
    if (!active)
    {
        set_monitor_active();
    }

    char ip[IP_BUFFER_SIZE];
    get_system_ip(ip, sizeof(ip));
    ip[strlen(ip) - 1] = '\0';
    snprintf(response, BUFFER_SIZE, "monitor http://%s:%d", ip, PORT);
    return 0;
}

void set_monitor_active()
{
    char ip[IP_BUFFER_SIZE];
    if (get_system_ip(ip, sizeof(ip)) < 0)
    {
        syslog(LOG_ERR, "failed to get system IP");
        return;
    }
    ip[strlen(ip) - 1] = '\0';
    monitor_args_t *args = malloc(sizeof(monitor_args_t));
    if (args == NULL)
    {
        syslog(LOG_ERR, "memory allocation failed");
        return;
    }

    strncpy(args->ip, ip, strlen(ip));
    args->port = PORT;
    args->duration = (time_t)(360000);
    active = true;

    pthread_t monitor_thread;
    if (pthread_create(&monitor_thread, NULL, execute_monitor, args) != 0)
    {
        free(args);
        syslog(LOG_ERR, "failed to create monitor thread");
        return;
    }

    pthread_detach(monitor_thread);
}
