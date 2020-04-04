// Name   : server.c
// Author : Modnar
// Date   : 2020/04/02

#include <sys/types.h>
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
#include <fcntl.h>

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

    struct sockaddr_in cli_addr;
    socklen_t client_addrlength = sizeof(cli_addr);
    int connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &client_addrlength);
    if (connfd < 0) {
        printf("Errno: %d\n", errno);
        close(sockfd);
    }

    char buffer[BUF_SIZE];
    fd_set read_fds, exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);
    while (1) {
        memset(buffer, '\0', sizeof(buffer));
        FD_SET(connfd, &read_fds);
        FD_SET(connfd, &exception_fds);
        ret = select(connfd+1, &read_fds, NULL, &exception_fds, NULL);
        if (ret < 0) {
            printf("Selection Failure.\n");
            break;
        }
        if (FD_ISSET(connfd, &read_fds)) {
            ret = recv(connfd, buffer, sizeof(buffer)-1, 0);
            if (ret <= 0)
                break;
            printf("Received(%dB): '%s'\n", ret, buffer);
        } else if (FD_ISSET(connfd, &exception_fds)) {
            ret = recv(connfd, buffer, sizeof(buffer)-1, MSG_OOB);
            if (ret <= 0)
                break;
            printf("MSG_OOB(%dB): '%s'\n", ret, buffer);
        }
    }
    close(connfd);
    close(sockfd);
    return EXIT_SUCCESS;
}
