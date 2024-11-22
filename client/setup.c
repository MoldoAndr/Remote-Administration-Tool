#include "client_daemon.h"

void daemon_init()
{
    pid_t pid = fork();

    if (pid < 0)
    {
        printf("Error: Unable to fork daemon process.\n");
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {
        exit(0);
    }

    if (setsid() < 0)
    {
        printf("Error: Unable to set session ID.\n");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0)
    {
        printf("Error: Unable to fork daemon process.\n");
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {
        exit(0);
    }

    umask(0);

    if (chdir("/") < 0)
    {
        printf("Error: Unable to change directory to root.\n");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_RDWR);

    openlog("client_daemon", LOG_PID, LOG_DAEMON);

    syslog(LOG_INFO, "Daemon started.");
}

void get_executable_path()
{
    char path_buffer[256];

    ssize_t len = readlink("/proc/self/exe", path_buffer, sizeof(path_buffer) - 1);
    if (len != -1)
    {
        path_buffer[len] = '\0';
        strncpy(executable_path, dirname(path_buffer), 256);
    }
    else
    {
        strncpy(executable_path, ".", 256);
    }
}

void signal_handler(int sig)
{
    if (sig == SIGTERM)
    {
        syslog(LOG_INFO, "Daemon terminated.");
        close(client_socket);
        exit(0);
    }
}
