#include <time.h>

#define SEC_TO_NANO 1000000000
#define NANO_TO_MILIS 1000000
#define MAX_SLEEP_TIME 25
#define MIN_SLEEP_TIME 75

typedef enum {
    WRITE = 0,
    READ
} operations;

typedef struct {
    int id;
    int fd;
    int n_threads;
    int port;
    char ip[16];
    operations mode;
} client_data_t;

typedef struct {
    int fd;
    int port;
    operations priority;
    int ratio;
} server_data_t;

typedef struct {
    operations action;
    unsigned int id;
} request_t;

typedef struct {
    operations action;
    unsigned int counter;
    long waiting_time;
} response_t;


void* client_thread(void* arg);
void* handle_client(void* client_data_ptr);
void start_server(server_data_t* server_data);
void write_counter();
void log_message(const char* message);
int connect_to_server(char* ip, int port);
void start_client(client_data_t* data);
void wait_time();
void send_response(int counter, operations action, int fd, int id, struct timespec start);

void with_ratio_readers(request_t request, int client_socket);
void with_ratio_writers(request_t request, int client_socket);

void no_ratio_readers(request_t request, int client_socket);
void no_ratio_writers(request_t request, int client_socket);
