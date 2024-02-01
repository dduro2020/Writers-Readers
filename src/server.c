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

void parse_args(int argc, char **argv, server_data_t *server_data) {
    int c;

    while (1) {
        int option_index = 0;

        static struct option long_options[] = {
            {"port", required_argument, NULL, 'p'},
            {"priority", required_argument, NULL, 'k'},
            {"ratio", optional_argument, NULL, 'r'},
            {0, 0, 0, 0}
        };

        c = getopt_long_only(argc, argv, "p:k:r::", long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
        case 'p':
            server_data->port = strtol(optarg, NULL, 10);
            break;

        case 'k':
            if (strcmp(optarg, "reader") == 0) {
                server_data->priority = READ;
            } else if (strcmp(optarg, "writer") == 0) {
                server_data->priority = WRITE;
            } else {
                fprintf(stderr, "error: incorrect priority, try: reader/writer\n");
                exit(EXIT_FAILURE);
            }
            break;

        case 'r':
            if (optarg) {
                server_data->ratio = strtol(optarg, NULL, 10);
            } else if (argv[optind] && argv[optind][0] != '-') {
                server_data->ratio = strtol(argv[optind], NULL, 10);
                optind++;  // Incrementar optind para omitir el valor de ratio en argv
            } else {
                server_data->ratio = -1;  // Valor predeterminado
            }
            break;


        case '?':
            exit(EXIT_FAILURE);

        default:
            printf("?? getopt returned character code 0%o ??\n", c);
        }
    }
}


int main(int argc, char *argv[]) {
    if (argc != 7 && argc != 5){
        fprintf(stderr, "%s --port $SERVER_PORT --priority $reader/writer --ratio(optional) $N\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    server_data_t server_data;
    parse_args(argc, argv, &server_data);
    start_server(&server_data);
    
}