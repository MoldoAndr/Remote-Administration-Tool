#include "serverlib.h"

pthread_mutex_t client_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_count = 0;
struct client_info *clients[MAX_CLIENTS];
char *log_folder = "client_logs";
char **commands = NULL;

int active_client_numbers[MAX_CLIENTS];
int num_active_clients = 0;
