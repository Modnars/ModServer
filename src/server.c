// Name   : server.c
// Author : Modnar
// Date   : 2020/04/02

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static const int BUF_SIZE = 1 << 10;

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return EXIT_FAILURE;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    int ret = bind(sockfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sockfd, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sockfd, (struct sockaddr *)&client, &client_addrlength);
    if (connfd < 0) {
        printf("Errno: %d\n", errno);
    } else {
        char buffer[BUF_SIZE];
        memset(buffer, '\0', BUF_SIZE);
        ret = recv(connfd, buffer, BUF_SIZE-1, 0);
        printf("RECV(%dB): '%s'\n", ret, buffer);
        
        memset(buffer, '\0', BUF_SIZE);
        ret = recv(connfd, buffer, BUF_SIZE-1, MSG_OOB);
        printf("OOB_DATA(%dB): '%s'\n", ret, buffer);

        memset(buffer, '\0', BUF_SIZE);
        ret = recv(connfd, buffer, BUF_SIZE-1, 0);
        printf("RECV(%dB): '%s'\n", ret, buffer);

        close(connfd);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
