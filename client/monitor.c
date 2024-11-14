#include "monitor.h"
#include "mongoose.h"
#include <pthread.h>

double g_cpu_usage = 0.0;
double g_mem_usage = 0.0;
long long g_disk_read = 0;
long long g_disk_write = 0;
long long g_net_rx = 0;
long long g_net_tx = 0;
double g_disk_usage = 0.0;

void system_monitor()
{
    CPUStats prev_cpu, curr_cpu;
    long total_mem, free_mem;
    long long prev_reads = 0, prev_writes = 0, curr_reads, curr_writes;
    NetStats prev_net, curr_net;

    while (1)
    {
        usleep(500000);

        get_cpu_usage(&prev_cpu, &curr_cpu);
        g_cpu_usage = calculate_cpu_usage(&prev_cpu, &curr_cpu);
        prev_cpu = curr_cpu;

        get_memory_usage(&total_mem, &free_mem);
        g_mem_usage = (total_mem - free_mem) * 100.0 / total_mem;

        get_disk_io(&curr_reads, &curr_writes);
        g_disk_read = curr_reads - prev_reads;
        g_disk_write = curr_writes - prev_writes;
        prev_reads = curr_reads;
        prev_writes = curr_writes;

        get_network_io(&curr_net);
        g_net_rx = curr_net.rx_bytes - prev_net.rx_bytes;
        g_net_tx = curr_net.tx_bytes - prev_net.tx_bytes;
        prev_net = curr_net;

        long total_disk, free_disk;
        get_disk_usage("/", &total_disk, &free_disk);
        g_disk_usage = (total_disk - free_disk) * 100.0 / total_disk;
    }
}

char *generate_json_data()
{
    static char json_output[256];
    snprintf(json_output, sizeof(json_output),
             "{\"cpu_usage\": %.2f, \"mem_usage\": %.2f, \"disk_read\": %lld, \"disk_write\": %lld, \"net_rx\": %lld, \"net_tx\": %lld, \"disk_usage\": %.2f}",
             g_cpu_usage, g_mem_usage, g_disk_read, g_disk_write,
             g_net_rx, g_net_tx, g_disk_usage);
    return json_output;
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;

        if (hm->uri.len == 5 && memcmp(hm->uri.buf, "/data", 5) == 0)
        {
            // Respond with JSON data
            char *json_data = generate_json_data();
            mg_http_reply(nc, 200, "Content-Type: application/json\r\n", json_data);
        }
        else
        {
            // Serve the main HTML page
            mg_http_reply(nc, 200, "Content-Type: text/html\r\n",
                          "<html>"
                          "<head><title>System Monitor</title>"
                          "<style>"
                          "body { font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 0; padding: 20px; }"
                          "h1 { color: #333; }"
                          "table { width: 100%; border-collapse: collapse; margin-top: 20px; }"
                          "th, td { border: 1px solid #ccc; padding: 8px; text-align: left; }"
                          "th { background-color: #eee; }"
                          "</style>"
                          "<script>"
                          "function updateData() {"
                          "  fetch('/data')"
                          "    .then(response => response.json())"
                          "    .then(data => {"
                          "      document.getElementById('cpu_usage').innerText = data.cpu_usage.toFixed(2) + '%';"
                          "      document.getElementById('mem_usage').innerText = data.mem_usage.toFixed(2) + '%';"
                          "      document.getElementById('disk_read').innerText = data.disk_read;"
                          "      document.getElementById('disk_write').innerText = data.disk_write;"
                          "      document.getElementById('net_rx').innerText = data.net_rx;"
                          "      document.getElementById('net_tx').innerText = data.net_tx;"
                          "      document.getElementById('disk_usage').innerText = data.disk_usage.toFixed(2) + '%';"
                          "    });"
                          "}"
                          "setInterval(updateData, 1000);"
                          "</script>"
                          "</head>"
                          "<body>"
                          "<h1>System Monitor</h1>"
                          "<table>"
                          "<tr><th>Metric</th><th>Value</th></tr>"
                          "<tr><td>CPU Usage</td><td id='cpu_usage'>0.00%</td></tr>"
                          "<tr><td>Memory Usage</td><td id='mem_usage'>0.00%</td></tr>"
                          "<tr><td>Disk I/O Read</td><td id='disk_read'>0</td></tr>"
                          "<tr><td>Disk I/O Write</td><td id='disk_write'>0</td></tr>"
                          "<tr><td>Network Rx</td><td id='net_rx'>0</td></tr>"
                          "<tr><td>Network Tx</td><td id='net_tx'>0</td></tr>"
                          "<tr><td>Disk Usage</td><td id='disk_usage'>0.00%</td></tr>"
                          "</table>"
                          "</body>"
                          "</html>");
        }
    }
}

void get_cpu_usage(CPUStats *prev, CPUStats *curr)
{
    int proc_fd = open(PROC_STAT, O_RDONLY); // Deschide fișierul /proc/stat
    if (proc_fd < 0)
    {
        write(2, "Error opening /proc/stat\n", 25); // Afișează eroare dacă nu poate deschide
        exit(1);
    }

    char buffer[BUF_SIZE]; // Buffer pentru stocarea datelor citite
    int bytes_read = read(proc_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';         // Termină șirul de caractere citit
        char *token = strtok(buffer, " "); // Tokenizează șirul pentru a găsi datele CPU
        if (token && strcmp(token, "cpu") == 0)
        {
            // Extrage și convertește datele de utilizare a CPU
            curr->user = atoll(strtok(NULL, " "));
            curr->nice = atoll(strtok(NULL, " "));
            curr->system = atoll(strtok(NULL, " "));
            curr->idle = atoll(strtok(NULL, " "));
        }
    }
    close(proc_fd); // Închide fișierul
}

double calculate_cpu_usage(CPUStats *prev, CPUStats *curr)
{
    long long prev_idle = prev->idle;
    long long curr_idle = curr->idle;
    long long prev_total = prev->user + prev->nice + prev->system + prev->idle;
    long long curr_total = curr->user + curr->nice + curr->system + curr->idle;
    long long total_diff = curr_total - prev_total;
    long long idle_diff = curr_idle - prev_idle;
    return (total_diff - idle_diff) * 100.0 / total_diff; // Returnează procentajul de utilizare a CPU
}

void get_memory_usage(long *total, long *free)
{
    int mem_fd = open(PROC_MEMINFO, O_RDONLY); // Deschide fișierul /proc/meminfo
    if (mem_fd < 0)
    {
        write(2, "Error opening /proc/meminfo\n", 28); // Afișează eroare dacă nu poate deschide
        exit(1);
    }

    char buffer[BUF_SIZE]; // Buffer pentru stocarea datelor citite
    int bytes_read = read(mem_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';         // Termină șirul de caractere citit
        char *line = strtok(buffer, "\n"); // Tokenizează fiecare linie
        while (line != NULL)
        {
            // Extrage totalul și memoria liberă din fișier
            if (strncmp(line, "MemTotal:", 9) == 0)
            {
                *total = atol(line + 10); // Totalul memoriei
            }
            else if (strncmp(line, "MemFree:", 8) == 0)
            {
                *free = atol(line + 9); // Memoria liberă
            }
            line = strtok(NULL, "\n"); // Continuă cu linia următoare
        }
    }
    close(mem_fd); // Închide fișierul
}

void get_disk_io(long long *reads, long long *writes)
{
    int disk_fd = open(PROC_DISKSTATS, O_RDONLY); // Deschide fișierul /proc/diskstats
    if (disk_fd < 0)
    {
        write(2, "Error opening /proc/diskstats\n", 30); // Afișează eroare dacă nu poate deschide
        exit(1);
    }

    char buffer[BUF_SIZE]; // Buffer pentru stocarea datelor citite
    *reads = 0;
    *writes = 0;
    int bytes_read;
    while ((bytes_read = read(disk_fd, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes_read] = '\0';         // Termină șirul de caractere citit
        char *line = strtok(buffer, "\n"); // Tokenizează fiecare linie
        while (line != NULL)
        {
            long long read, write;
            // Extrage valorile de citire/scriere pentru fiecare linie
            int fields = sscanf(line, "%*d %*d %*s %lld %*d %*d %*d %lld", &read, &write);
            if (fields == 2)
            {
                *reads += read;   // Adaugă la totalul citirilor
                *writes += write; // Adaugă la totalul scrierilor
            }
            line = strtok(NULL, "\n"); // Continuă cu linia următoare
        }
    }
    close(disk_fd); // Închide fișierul
}

void get_network_io(NetStats *stats)
{
    int net_fd = open(PROC_NET_DEV, O_RDONLY); // Deschide fișierul /proc/net/dev
    if (net_fd < 0)
    {
        write(2, "Error opening /proc/net/dev\n", 28); // Afișează eroare dacă nu poate deschide
        exit(1);
    }

    char buffer[BUF_SIZE]; // Buffer pentru stocarea datelor citite
    int bytes_read = read(net_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';         // Termină șirul de caractere citit
        char *line = strtok(buffer, "\n"); // Tokenizează fiecare linie
        stats->rx_bytes = 0;
        stats->tx_bytes = 0;
        while (line != NULL)
        {
            if (strstr(line, ":") != NULL) // Salt peste liniile de antet
            {
                char iface[20];
                long long rx, tx;
                sscanf(line, "%s %lld %*d %*d %*d %*d %*d %*d %*d %lld", iface, &rx, &tx);
                if (strcmp(iface, "lo:") != 0) // Exclude loopback-ul
                {
                    stats->rx_bytes += rx; // Adaugă la totalul de date recepționate
                    stats->tx_bytes += tx; // Adaugă la totalul de date transmise
                }
            }
            line = strtok(NULL, "\n"); // Continuă cu linia următoare
        }
    }
    close(net_fd); // Închide fișierul
}

void get_disk_usage(const char *path, long *total, long *free)
{
    struct statvfs stat; // Structură pentru statistici de sistem de fișiere
    if (statvfs(path, &stat) != 0)
    {
        write(2, "Error getting disk usage\n", 25); // Afișează eroare dacă nu poate obține datele
        exit(1);
    }
    *total = stat.f_blocks * stat.f_frsize; // Total spațiu pe disc
    *free = stat.f_bfree * stat.f_frsize;   // Spațiu liber pe disc
}

void run_system_monitor_server(char *ip_address, int port, time_t duration)
{
    struct mg_mgr mgr;
    struct mg_connection *nc;

    mg_mgr_init(&mgr);

    char url[30];
    snprintf(url, sizeof(url), "http://%s:%d", ip_address, port);

    nc = mg_http_listen(&mgr, url, ev_handler, NULL);

    if (nc == NULL)
    {
        printf("Failed to create listener\n");
        return;
    }

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, (void *(*)(void *))system_monitor, NULL);

    time_t start_time = time(NULL);

    for (;;)
    {
        if (time(NULL) - start_time >= duration)
        {
           break;
        }
        mg_mgr_poll(&mgr, 600);
    }

    mg_mgr_free(&mgr);
}
