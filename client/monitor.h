#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <pthread.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define PROC_STAT "/proc/stat"
#define PROC_MEMINFO "/proc/meminfo"
#define PROC_DISKSTATS "/proc/diskstats"
#define PROC_NET_DEV "/proc/net/dev"
#define BUF_SIZE 1024
#define MAX_BUFFER 8192
#define MAX_PROCESSES 10

typedef struct
{
    long long user;
    long long nice;
    long long system;
    long long idle;
} CPUStats;

typedef struct
{
    long long rx_bytes;
    long long tx_bytes;
} NetStats;

typedef struct
{
    pid_t pid;
    pid_t ppid;
    double cpu_usage;
    double mem_usage;
    int num_threads;
    char user[32];
} ProcessInfo;

extern double g_cpu_usage;
extern double g_mem_usage;
extern long long g_disk_read;
extern long long g_disk_write;
extern long long g_net_rx;
extern long long g_net_tx;
extern double g_disk_usage;

extern ProcessInfo g_top_processes[MAX_PROCESSES];

void get_cpu_usage(CPUStats *curr);

void get_top_processes();

double calculate_cpu_usage(CPUStats *prev, CPUStats *curr);

void get_memory_usage(long *total, long *free);

void get_disk_io(long long *reads, long long *writes);

void get_network_io(NetStats *stats);

void get_disk_usage(const char *path, long *total, long *free);

void* system_monitor();

char *generate_json_data();

void run_system_monitor_server(char *ip, int port, time_t duration);
