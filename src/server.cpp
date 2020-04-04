// Name   : server.cpp
// Author : Modnar
// Date   : 2020/04/04

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define USER_LIMIT          10
#define BUFFER_SIZE         1024
#define FD_LIMIT            65535
#define MAX_EVENT_NUMBER    1024
#define PROCESS_LIMIT       65535

// 处理一个客户连接必要的数据
struct client_data {
    sockaddr_in address;    /* 客户端socket地址 */
    int connfd;             /* socket文件描述符 */
    pid_t pid;              /* 处理这个连接的子进程为PID */
    int pipefd[2];          /* 和父进程通信用的管道 */
};

// 子进程数组，用进程PID来进行索引，获得相关用户数据
int *sub_process = NULL;
// 记录当前用户总数
int user_count = 0;
client_data *users = NULL;
bool stop_child = false;

// 将文件描述符fd设置为非阻塞，以便于IO复用
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 标识停止一个子进程
void child_term_handler(int sig) {
    stop_child = true;
}

/**
 * 检查fd是否是一个合法文件描述符
 *   fd: 待检查文件描述符
 *   errMsg: 日志信息(创建失败)
 *   successMsg: 日志信息(创建成功)
 * 若fd非法，打印失败日志并终止当前进程; 否则打印创建成功日志信息
 */
void check(int fd, const char *errMsg, const char *successMsg) {
    if (fd < 0) {
        perror(errMsg);
        exit(EXIT_FAILURE);
    } else {
        printf("%s\n", successMsg);
    }
}

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        // 处理程序传入参数
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return EXIT_FAILURE;
    }
    // 转换并存储IP、PORT数据
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    char serverMsg[BUFFER_SIZE], recvMsg[BUFFER_SIZE];

    // 初始化Server地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    // 建立通信socket
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    check(sockfd, "[FAILED]: AT CREATE SOCKET", "[OK]: AT CREATE SOCKET");

    // 将server地址与socket绑定
    int ret = bind(sockfd, (struct sockaddr *)&address, sizeof(address));
    check(ret, "[FAILED]: AT CREATE BIND", "[OK]: AT CREATE BIND");

    // 进行监听
    ret = listen(sockfd, 5);
    check(ret, "[FAILED]: AT CREATE LISTEN", "[OK]: AT CREATE LISTEN");

    struct timespec timeout = {10, 0};
    user_count = 0;
    users = new client_data[USER_LIMIT+1];

    int kq = kqueue();
    check(kq, "[FAILED]: AT CREATE KQUEUE", "[OK]: AT CREATE KQUEUE");

    struct kevent changes;
    // TODO
    EV_SET(&changes, STDIN_FILENO, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &changes, 1, NULL, 0, NULL);

    // TODO
    EV_SET(&changes, sockfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &changes, 1, NULL, 0, NULL);

    // TODO
    struct kevent events[USER_LIMIT];

    while (true) {
        ret = kevent(kq, NULL, 0, events, 10, &timeout);
        if (ret < 0) {
            printf("[FAILED]: AT KEVENT\n"); 
            continue;
        } else if (ret == 0) {
            printf("[TIMEOUT]: KEVENT TIMEOUT\n");
            continue;
        } else {
            printf("[OK]: AT KEVENT\n");
            for (int i = 0; i < ret; ++i) {
                struct kevent curr_event = events[i];
                if (curr_event.ident == STDIN_FILENO) {
                    bzero(serverMsg, BUFFER_SIZE);
                    fgets(serverMsg, BUFFER_SIZE, stdin);
                    // 手动关闭服务器
                    if (strncmp(serverMsg, "exit", 4) == 0) {
                        exit(EXIT_SUCCESS);
                    }
                    // 向所有客户端连接发送该消息
                    for (int i = 0; i < USER_LIMIT; ++i) {
                        if (users[i].connfd != 0) {
                            printf("[LOG]: SendTo(%d): %s\n", users[i].connfd, 
                                    serverMsg);
                            send(users[i].connfd, serverMsg, BUFFER_SIZE, 0);
                        }
                    }
                } else if (curr_event.ident == sockfd) { // 建立新的连接
                    // 创建客户端地址
                    struct sockaddr_in cli_addr;
                    socklen_t cli_addrlen = sizeof(cli_addr);
                    int connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_addrlen);
                    if (connfd < 0) {
                        printf("[ErrNo.%d]: %s\n", errno, strerror(errno));
                    } else {
                        int idx = -1;
                        // 设置用户的connfd
                        for (int i = 0; i < USER_LIMIT; ++i) {
                            if (users[i].connfd == 0) {
                                idx = i;
                                users[i].connfd = connfd;
                                break;
                            }
                        }
                        if (idx >= 0) {
                            // 监听该客户端输入
                            EV_SET(&changes, connfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                            // 放入事件队列
                            kevent(kq, &changes, 1, NULL, 0, NULL);
                            printf("[NEW_USER(%d)]: JOIN SUCCESSFULLY\n", connfd);
                            // 可使用cli_addr来输出用户IP:PORT
                        } else {
                            bzero(serverMsg, BUFFER_SIZE);
                            strcpy(serverMsg, "[ERROR]: THE USERS COUNT GOT FULL!\n");
                            send(connfd, serverMsg, BUFFER_SIZE, 0);
                            printf("[FAILED]: THE NEW CLIENT(%s:%d) JOINED FAILED\n",
                                    inet_ntoa(cli_addr.sin_addr), 
                                    ntohs(cli_addr.sin_port));
                        }
                    }
                } else { // 用户发来消息
                    bzero(recvMsg, BUFFER_SIZE);
                    ret = recv((int)curr_event.ident, recvMsg, BUFFER_SIZE, 0);
                    if (ret > 0) {
                        printf("[CLIENT(%d)]: %s\n", (int)curr_event.ident, recvMsg);
                    } else if (ret < 0) {
                        printf("[ERROR]: AT RECEIVED MESSAGE FROM CLINET(%d)\n",
                                (int)curr_event.ident);
                    } else {
                        EV_SET(&changes, curr_event.ident, EVFILT_READ, EV_DELETE,
                                0, 0, NULL);
                        kevent(kq, &changes, 1, NULL, 0, NULL);
                        close((int)curr_event.ident);
                        for (int i = 0; i < USER_LIMIT; ++i) {
                            if (users[i].connfd == (int)curr_event.ident) {
                                users[i].connfd = 0;
                                break;
                            }
                        }
                        printf("[EXIT]: CLINET(%d) EXIT.\n", (int)curr_event.ident);
                    }
                }
            }
        }
    }
    // close(connfd); // 关闭连接
    // close(sockfd); // 关闭socket
    return EXIT_SUCCESS;
}
