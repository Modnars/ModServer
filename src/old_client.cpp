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

const int HISTORY_MSG_NUM   = 10;
const int USER_INFO_SIZE    = 64;
const int MESSAGE_SIZE      = 256;
const int BUFFER_SIZE       = USER_INFO_SIZE + MESSAGE_SIZE;

char *buffer;

void print_history_msg() {
    for (int pos = 0; pos < HISTORY_MSG_NUM; ++pos) {
        if (strlen(buffer+pos*BUFFER_SIZE) > 0) {
            printf("%s\n", buffer+pos*BUFFER_SIZE); 
        } else {
            break;
        }
    }
}

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

    char userMsg[MESSAGE_SIZE], recvMsg[MESSAGE_SIZE];
    buffer = new char[HISTORY_MSG_NUM*BUFFER_SIZE];

    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("connection failed\n");
    } else {
        bzero(userMsg, MESSAGE_SIZE);
        recv(sockfd, recvMsg, MESSAGE_SIZE, 0);
        printf("%s\n", recvMsg);
        while (true) {
            printf(">> ");
            bzero(userMsg, MESSAGE_SIZE);
            bzero(recvMsg, MESSAGE_SIZE);
            fgets(userMsg, MESSAGE_SIZE, stdin);
            if (strncmp(userMsg, "exit", 4) == 0) break;
            if (userMsg[strlen(userMsg)-1] == '\n')
                userMsg[strlen(userMsg)-1] = '\0';
            // 这里仅考虑客户端处于稳定的网络环境下，因此并未对下列函数进行异常分析
            send(sockfd, userMsg, strlen(userMsg), 0);
            // if (strncmp(userMsg, "history", strlen("history")) == 0) {
            if (strlen(userMsg) > 0 && userMsg[0] == '$') {
                bzero(buffer, HISTORY_MSG_NUM*BUFFER_SIZE);
                recv(sockfd, buffer, HISTORY_MSG_NUM*BUFFER_SIZE, 0);
                print_history_msg();
                continue;
            }
            recv(sockfd, recvMsg, MESSAGE_SIZE, 0);
            printf("%s\n", recvMsg);
        }
    }
    close(sockfd);
    return EXIT_SUCCESS;
}
