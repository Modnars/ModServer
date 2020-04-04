// Name   : client.c
// Author : Modnar
// Date   : 2020/04/02

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return EXIT_FAILURE;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    char userMsg[BUFFER_SIZE];
    if (connect(sockfd, (struct sockaddr *)&server_address, 
                sizeof(server_address)) < 0) {
        printf("connection failed\n");
    } else {
        while (true) {
            printf(">> ");
            bzero(userMsg, BUFFER_SIZE);
            fgets(userMsg, BUFFER_SIZE, stdin);
            if (strncmp(userMsg, "exit", 4) == 0) break;
            send(sockfd, userMsg, strlen(userMsg), 0);
        }
    }
    close(sockfd);
    return EXIT_SUCCESS;
}
