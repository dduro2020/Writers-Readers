#include "proxy.h" 
#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <err.h>
#include <getopt.h>
#include <signal.h>
#define _GNU_SOURCE

int my_fd;

void control() {
    close(my_fd);
}

void parse_args(int argc, char **argv, client_data_t *client_data){
    int c;
    while (1) {
        int option_index = 0;

        static struct option long_options[] = {
            {"port", required_argument, NULL,'p'},
            {"ip", required_argument, NULL,'i'},
            {"mode", required_argument, NULL,'m'},
            {"threads", required_argument, NULL,'t'},
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "",
                    long_options, &option_index);
        
        if (c == -1)
            break;

        switch (c) {
        case 'i':
            strncpy(client_data->ip, optarg, 16);
            break;

        case 'p':
            client_data->port = strtol(optarg, NULL, 10);
            break;

        case 'm':
            if(strcmp(optarg, "reader") == 0){
                client_data->mode = READ;
            }else if(strcmp(optarg, "writer") == 0){
                client_data->mode = WRITE;
            }else{
                fprintf(stderr, "error: incorrect mode, try: reader/writer\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            client_data->n_threads = strtol(optarg, NULL, 10);
            break;

        case '?':
            exit(EXIT_FAILURE);

        default:
            printf("?? getopt returned character code 0%o ??\n", c);
            
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 9){
        fprintf(stderr, "%s --ip $CLIENT_IP --port $CLIENT_PORT --mode $reader/writer --threads $number\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    client_data_t client_data;
    parse_args(argc, argv, &client_data);
    start_client(&client_data);
}