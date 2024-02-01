#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include "proxy.h"

#define MAX_CLIENTS 250
#define BACKLOG 1024

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t aux_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t readers_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writers_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t sem;
sem_t sem_aux;
sem_t ratio_sem;
sem_t threads_sem;

int ratio_readers = 0;
int ratio_writers = 0;

int reader_count = 0;
int writer_count = 0;

int ratio = 0;
int priority_type = 0;

unsigned int counter = 0;

void log_message(const char* message) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long seconds = ts.tv_sec;
    long nanoseconds = ts.tv_nsec;
    printf("[%ld.%09ld] %s\n", seconds, nanoseconds, message);
}

void write_counter() {
    FILE *file = fopen("server_output.txt", "w");
    if (file != NULL) {
        fprintf(file, "%d\n", counter);
        fclose(file);
    }
}

void wait_time() {
    struct timespec time_sleep;
    
    srand(time(NULL)); //random sleep between 25 - 75 miliseconds
    int rand_num = rand () % (MAX_SLEEP_TIME - MIN_SLEEP_TIME+1) + MIN_SLEEP_TIME;
    time_sleep.tv_sec = 0;
    time_sleep.tv_nsec = rand_num * NANO_TO_MILIS;
    nanosleep(&time_sleep, NULL);
}

void send_response(int counter, operations action, int fd, int id, struct timespec start) {
    response_t response;
    char aux[500];
    struct timespec end;
    memset(&response, 0, sizeof(response_t));
    
    response.action = action;
    response.counter = counter;
    
    if (action == READ) {
        sprintf(aux, "[LECTOR #%d] lee contador con valor %d", id, counter);
    } else {
        sprintf(aux, "[ESCRITOR #%d] modifica contador con valor %d", id, counter);
    }
    
    log_message(aux);
    
    clock_gettime(CLOCK_MONOTONIC, &end);

    response.waiting_time = (end.tv_sec - start.tv_sec) * SEC_TO_NANO; // Diferencia en segundos a nanosegundos

    // Ajustar la diferencia de nanosegundos para evitar resultados negativos
    long nsec_diff = end.tv_nsec - start.tv_nsec;
    if (nsec_diff < 0) {
        nsec_diff += SEC_TO_NANO; // Sumar 1 segundo en nanosegundos
    }

    response.waiting_time += nsec_diff; // Agregar la diferencia en nanosegundos

    send(fd, &response, sizeof(response_t), 0);
}

void sem_post_nt(int n, sem_t *sem) {
    int i;
    for (i = 0; i < n; i++) {
        sem_post(sem);
    }
}

void no_ratio_readers(request_t request, int client_socket) {
    struct timespec start;
    int reader_id = request.id;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    if (priority_type == READ) {
        
        pthread_mutex_lock(&readers_mutex);
        reader_count++;
        if (reader_count == 1) {
            sem_wait(&sem);
        }
        pthread_mutex_unlock(&readers_mutex);

        wait_time();
        send_response(counter, request.action, client_socket, reader_id, start);

        pthread_mutex_lock(&readers_mutex);
        reader_count--;
        if (reader_count == 0) {
            sem_post(&sem);
        }
        pthread_mutex_unlock(&readers_mutex);

    
    } else {
        pthread_mutex_lock(&readers_mutex);
        sem_wait(&ratio_sem);
        pthread_mutex_lock(&mutex);
        reader_count++;
        if (reader_count == 1) {
            sem_wait(&sem);
        }
        pthread_mutex_unlock(&mutex);
        sem_post(&ratio_sem);
        pthread_mutex_unlock(&readers_mutex);

        wait_time();
        send_response(counter, request.action, client_socket, reader_id, start);

        pthread_mutex_lock(&mutex);
        reader_count--;
        if (reader_count == 0) {
            sem_post(&sem);
        }
        pthread_mutex_unlock(&mutex);
    }
}

void no_ratio_writers(request_t request, int client_socket) {
    struct timespec start;
    int writer_id = request.id;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    if (priority_type == READ) {
        
        pthread_mutex_lock(&writers_mutex);
        sem_wait(&sem);
        
        wait_time();
        counter++;
        write_counter();
        send_response(counter, request.action, client_socket, writer_id, start);

        sem_post(&sem);
        pthread_mutex_unlock(&writers_mutex);

    
    } else {
        pthread_mutex_lock(&writers_mutex);
        writer_count++;
        if (writer_count == 1) {
            sem_wait(&ratio_sem);
        }
        pthread_mutex_unlock(&writers_mutex);
        sem_wait(&sem);

        wait_time();
        counter++;
        write_counter();
        send_response(counter, request.action, client_socket, writer_id, start);

        sem_post(&sem);

        pthread_mutex_lock(&writers_mutex);
        writer_count--;
        if (writer_count == 0) {
            sem_post(&ratio_sem);
        }
        pthread_mutex_unlock(&writers_mutex);
    }
}

void with_ratio_readers(request_t request, int client_socket) {
    struct timespec start;
    int reader_id = request.id;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    if (priority_type == READ) {
        
        pthread_mutex_lock(&readers_mutex);
        reader_count++;
        ratio_readers++;
        if (reader_count == 1) {
            sem_wait(&sem);
        }
        if (ratio_readers == ratio) {
            ratio_readers = 0;
            sem_post(&sem);
            wait_time();  /* minima espera para dar tiempo a los escritores a pasar */
            sem_wait(&sem);
        }
        pthread_mutex_unlock(&readers_mutex);

        wait_time();
        send_response(counter, request.action, client_socket, reader_id, start);

        pthread_mutex_lock(&readers_mutex);
        reader_count--;
        
        if (reader_count == 0) {
            sem_post(&sem);
        }
        pthread_mutex_unlock(&readers_mutex);

    
    } else {
        pthread_mutex_lock(&readers_mutex);
        wait_time();  /* minima espera para dar tiempo a los escritores a pasar */
        sem_wait(&ratio_sem);
        pthread_mutex_lock(&mutex);
        reader_count++;
        if (reader_count == 1) {
            sem_wait(&sem);
        }
        pthread_mutex_unlock(&mutex);
        
        pthread_mutex_unlock(&readers_mutex);

        wait_time();
        send_response(counter, request.action, client_socket, reader_id, start);

        pthread_mutex_lock(&mutex);
        sem_post(&ratio_sem);
        reader_count--;
        if (reader_count == 0) {
            sem_post(&sem);
        }
        pthread_mutex_unlock(&mutex);
    }
}

void with_ratio_writers(request_t request, int client_socket) {
    struct timespec start;
    int writer_id = request.id;
    struct timespec time_sleep;
    
    time_sleep.tv_sec = 0;
    time_sleep.tv_nsec = 8 * NANO_TO_MILIS;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    if (priority_type == READ) {
        
        pthread_mutex_lock(&writers_mutex);
        sem_wait(&sem);
        
        wait_time();
        counter++;
        write_counter();
        send_response(counter, request.action, client_socket, writer_id, start);
        
        sem_post(&sem);
        pthread_mutex_unlock(&writers_mutex);

    
    } else {
        pthread_mutex_lock(&writers_mutex);
        writer_count++;
        if (writer_count == 1) {
            sem_wait(&ratio_sem);
        }
        pthread_mutex_unlock(&writers_mutex);
        
        pthread_mutex_lock(&aux_mutex);
        sem_wait(&sem);
        
        if (ratio_writers == ratio) {
            ratio_writers = 0;
            sem_post(&ratio_sem);
            nanosleep(&time_sleep, NULL);  /* minima espera para dar tiempo a los lectores a pasar */
            sem_post(&sem);
            sem_wait(&ratio_sem);
        
        }
        wait_time();
        counter++;
        write_counter();
        send_response(counter, request.action, client_socket, writer_id, start);

        sem_post(&sem);
        ratio_writers++;
        pthread_mutex_unlock(&aux_mutex);
        
        pthread_mutex_lock(&writers_mutex);
        
        writer_count--;
        if (writer_count == 0) {
            sem_post(&ratio_sem);
        }
        pthread_mutex_unlock(&writers_mutex);
    }
}

void* handle_client(void* client_data_ptr) {
    client_data_t* client_data = (client_data_t*)client_data_ptr;
    int client_socket = client_data->fd;
    request_t request;
    ssize_t bytes_read;

    bytes_read = recv(client_socket, &request, sizeof(request_t), 0);
    if (bytes_read < 0) {
        perror("Error reading from socket");
        close(client_socket);
        return NULL;
    } else if (bytes_read == 0) {
        close(client_socket);
        return NULL;
    }
    sem_wait(&threads_sem);
    if (ratio > 0) {
        if (request.action == READ) {
            with_ratio_readers(request, client_socket);
        } else {
            with_ratio_writers(request, client_socket);
        }
    } else {
        if (request.action == READ) {
            no_ratio_readers(request, client_socket);
        } else {
            no_ratio_writers(request, client_socket);
        }
        
    }

    close(client_socket);
    sem_post(&threads_sem);

    return NULL;
}

void start_server(server_data_t* server_data) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    unsigned int client_len = sizeof(client_addr);
    int enable = 1;

    ratio = server_data->ratio;
    priority_type = server_data->priority;
    if (ratio > 0) {
        sem_init(&sem, 0, 1);
        sem_init(&ratio_sem, 0, 1);
        sem_init(&sem_aux, 0, 0);
    } else {
        sem_init(&sem, 0, 1);
        sem_init(&ratio_sem, 0, 1);
    }
    sem_init(&threads_sem, 0, MAX_CLIENTS);

    setbuf(stdout, NULL);
    
    // Crear el socket del servidor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error opening server socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    // Configurar el socket del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(server_data->port);

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("Error: setsockopt(SO_REUSEADDR) failed\n");
        exit(1);
    }

    // Asociar el socket del servidor a la dirección y puerto
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding server socket");
        exit(1);
    }

    // Escuchar en el socket del servidor
    if (listen(server_socket, BACKLOG) < 0) {
        perror("Error listening on server socket");
        exit(1);
    }

    //printf("Server started listening on port %d\n", server_data->port);

    while (1) {
        // Aceptar la conexión del cliente
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error accepting client connection");
            exit(1);
        }

        pthread_t tid;
        client_data_t* client_data = malloc(sizeof(client_data_t));
        client_data->fd = client_socket;
        // Crear un hilo para manejar la comunicación con el cliente
        if (pthread_create(&tid, NULL, handle_client, client_data) != 0) {
            perror("Error creating thread");
            exit(1);
        }
    }

    // Cerrar el socket del servidor
    close(server_socket);
}

int connect_to_server(char* ip, int port) {
    // Create client socket
    int client_fd;
    struct sockaddr_in server_addr;
    
    setbuf(stdout, NULL);
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        return -1;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    if (connect(client_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        return -1;
    }

    return client_fd;
}

void* client_thread(void* arg) {
    client_data_t* data = (client_data_t*) arg;
    // Create request
    request_t request;
    request.action = data->mode;
    request.id = data->id;
    // Connect to server
    data->fd = connect_to_server(data->ip, data->port);
    //printf("CAMPOS CLIENTE: port:%d, ip:%s, mode:%d, socket:%d\n", data->port, data->ip, data->mode, data->fd);
    if (data->fd == -1) {
        perror("Error connecting socket");
        return NULL;
    }
    // Send request to server
    send(data->fd, &request, sizeof(request_t), 0);

    // Receive response from server
    response_t response;
    recv(data->fd, &response, sizeof(response_t), 0);

    // Print response
    printf("[Client #%d] Reader/Writer, counter=%d, time=%ld ns.\n", data->id, response.counter, response.waiting_time);
    
    close(data->fd);
    
    return NULL;
}

void start_client(client_data_t* data) {

    // Create client threads
    pthread_t* client_tids = malloc(data->n_threads * sizeof(pthread_t));
    client_data_t* client_data_array = malloc(data->n_threads * sizeof(client_data_t));  // Array para almacenar los datos de cada hilo
    int i;
    for (i = 0; i < data->n_threads; i++) {
        client_data_t* data_aux = &client_data_array[i];  // Obtener el puntero al elemento correspondiente del array
        data_aux->id = i;
        data_aux->mode = data->mode;
        data_aux->fd = 0;
        data_aux->port = data->port;
        strncpy(data_aux->ip, data->ip, strlen(data->ip)+1);
        pthread_create(&client_tids[i], NULL, client_thread, data_aux);
    }

    // Wait for client threads to finish
    for (i = 0; i < data->n_threads; i++) {
        pthread_join(client_tids[i], NULL);
    }

    // Clean up
    free(client_data_array);
    free(client_tids);

}
