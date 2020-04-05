// Name   : server.cpp
// Author : Modnar
// Date   : 2020/04/04

#include <iostream>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/mman.h>
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

static const char *shm_name = "/my_shm";
int shmfd;
int listenfd;
int sig_pipefd[2];
int kq;
char *share_mem = NULL;
// 客户连接数组，进程使用客户连接的编号来索引，获得相关用户数据
client_data *users = NULL;
// 子进程数组，用进程PID来进行索引，获得相关用户数据
int *sub_process = NULL;
// 记录当前用户总数
int user_count = 0;
// 标识是否关闭子进程
bool stop_child = false;

// 将文件描述符fd设置为非阻塞，以便于IO复用
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 添加监测文件描述符到changes中，监测的选项由用户自定义
void addfd(int kq, struct kevent *changes, int *fd, int option) {
    EV_SET(changes, *fd, option, EV_ADD, 0, 0, fd);
    setnonblocking(*fd);
}

void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig, void (*handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 在服务器停止关闭时回收资源
void del_resource() {
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(kq); // TODO
    shm_unlink(shm_name);
    delete [] users;
    delete [] sub_process;
}

// 停止一个子进程
void child_term_handler(int sig) {
    stop_child = true;
}

/**
 * 检查fd是否是一个合法文件描述符
 *   success: 判断结果
 *   errMsg: 日志信息(创建失败)
 *   successMsg: 日志信息(创建成功)
 * 若fd非法，打印失败日志并终止当前进程; 否则打印创建成功日志信息
 */
void check(bool success, const char *errMsg, const char *successMsg) {
    if (!success) {
        perror(errMsg);
        exit(EXIT_FAILURE);
    } else {
        printf("%s\n", successMsg);
    }
}

/**
 * 子进程运行函数
 *   idx: 子进程处理的客户连接编号
 *   users: 保存所有客户连接数据的数组
 *   share_mem: 共享内存起始地址
 */
int run_child(int idx, client_data *users, char *share_mem) {
    struct kevent changes, events[MAX_EVENT_NUMBER];
    int child_kq = kqueue();
    assert(child_kq != -1);
    int connfd = users[idx].connfd;
    // addfd(child_kq, changes, &connfd, EVFILT_READ); // TODO
    EV_SET(&changes, connfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &changes, 1, NULL, 0, NULL);
    int pipefd = users[idx].pipefd[1];
    // addfd(child_kq, changes, &pipefd, EVFILT_READ); // TODO
    EV_SET(&changes, pipefd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &changes, 1, NULL, 0, NULL);
    int ret;
    addsig(SIGTERM, child_term_handler, false);

    while (!stop_child) {
        // int number = kevent(kq, changes, MAX_EVENT_NUMBER, events, MAX_EVENT_NUMBER, NULL);
        int number = kevent(kq, NULL, 0, events, MAX_EVENT_NUMBER, NULL);
        if ((number < 0) && (errno != EINTR)) { // TODO
            printf("KQUEUE FAILED\n");
            break;
        }
        for (int i = 0; i < number; ++i) {
            struct kevent curr_event = events[i];
            int sockfd = curr_event.ident;
            if ((sockfd == connfd) && (curr_event.flags & EV_ENABLE)) { // TODO
                memset(share_mem+idx*BUFFER_SIZE, '\0', BUFFER_SIZE);
                ret = recv(connfd, share_mem+idx*BUFFER_SIZE, BUFFER_SIZE-1, 0);
                if (ret < 0) {
                    if (errno != EAGAIN) {
                        stop_child = true;
                    }
                } else if (ret == 0) {
                    stop_child = true;
                } else {
                    send(pipefd, (char *)&idx, sizeof(idx), 0);
                }
            } else if ((sockfd == pipefd) && (curr_event.flags & EV_ENABLE)) {
                int client = 0;
                ret = recv(sockfd, (char *)&client, sizeof(client), 0);
                if (ret < 0) {
                    if (errno != EAGAIN) {
                        stop_child = true;
                    }
                } else if (ret == 0) {
                    stop_child = true;
                } else {
                    send(connfd, share_mem+idx*BUFFER_SIZE, BUFFER_SIZE, 0);
                }
            } else {
                continue;
            }
        }
    }
    close(connfd);
    close(pipefd);
    close(child_kq);
    return EXIT_SUCCESS;
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

    int ret;
    // 初始化server地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    // 建立通信socket
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    check((listenfd != -1), "[FAILED]: AT CREATE SOCKET", "[OK]: AT CREATE SOCKET");

    // 将server地址与socket绑定
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    check((ret != -1), "[FAILED]: AT CREATE BIND", "[OK]: AT CREATE BIND");

    // 进行监听
    ret = listen(listenfd, USER_LIMIT);
    check((ret != -1), "[FAILED]: AT CREATE LISTEN", "[OK]: AT CREATE LISTEN");

    // 初始化TIME_OUT设定
    struct timespec timeout = {10, 0};
    user_count = 0;
    users = new client_data[USER_LIMIT+1];
    sub_process = new int[PROCESS_LIMIT];
    for (int i = 0; i < PROCESS_LIMIT; ++i)
        sub_process[i] = -1;

    // 获得kqueue
    kq = kqueue();
    check((kq != -1), "[FAILED]: AT CREATE KQUEUE", "[OK]: AT CREATE KQUEUE");
    struct kevent changes, events[MAX_EVENT_NUMBER];
    EV_SET(&changes, listenfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &changes, 1, NULL, 0, NULL);
    // addfd(kq, changes, &listenfd, EVFILT_READ); // TODO

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);
    EV_SET(&changes, sig_pipefd[1], EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &changes, 1, NULL, 0, NULL);
    // addfd(kq, &changes, &sig_pipefd[0], EVFILT_READ);

    // 注册信号处理函数
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
    bool stop_server = false;
    bool terminate = false;

    // shm_unlink(shm_name);
    shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    assert(shmfd != -1);
    ret = ftruncate(shmfd, USER_LIMIT*BUFFER_SIZE);
    // std::cout << shmfd << strerror(errno) << std::endl;
    assert(ret != -1);

    share_mem = (char *)mmap(NULL, USER_LIMIT*BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    assert(share_mem != MAP_FAILED);
    close(shmfd);

    while (!stop_server) {
        stop_server = true; // TODO TEST
        int number = kevent(kq, NULL, 0, events, MAX_EVENT_NUMBER, &timeout);
        std::cout << "Number: " << number << std::endl;
        if ((number < 0) && (errno != EINTR)) {
            printf("[FAILED]: AT KEVENT\n"); 
            break;
        }
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].ident;
            if (sockfd == listenfd) { // 新建连接
                struct sockaddr_in cli_addr;
                socklen_t cli_addrlen = sizeof(cli_addr);
                int connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_addrlen);
                if (connfd < 0) {
                    printf("[ErrNo.%d]: %s\n", errno, strerror(errno));
                    continue;
                }
                if (user_count >= USER_LIMIT) {
                    const char *info = "[FAILED]: Too many users!\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                }
                users[user_count].address = cli_addr;
                users[user_count].connfd = connfd;
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                assert(ret != -1);
                pid_t pid = fork(); // 创建子进程 优化: 使用进程池(TODO)
                if (pid < 0) {
                    close(connfd);
                    continue;
                } else if (pid == 0) { // 子进程
                    close(kq);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    run_child(user_count, users, share_mem);
                    munmap((void *)share_mem, USER_LIMIT*BUFFER_SIZE);
                    exit(EXIT_SUCCESS);
                } else {
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    // TODO
                    // addfd(kq, &changes, &users[user_count].pipefd[0], EVFILT_READ);
                    EV_SET(&changes, users[user_count].pipefd[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
                    kevent(kq, &changes, 1, NULL, 0, NULL);
                    users[user_count].pid = pid;
                    sub_process[pid] = user_count;
                    ++user_count;
                }
            } else if ((sockfd == sig_pipefd[0]) && (events[i].flags & EV_ENABLE)) { // TODO
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {
                            case SIGCHLD:
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                    int del_user = sub_process[pid];
                                    sub_process[pid] = -1;
                                    if ((del_user < 0) || (del_user > USER_LIMIT)) {
                                        continue;
                                    }
                                    // TODO 停止kqueue对该用户进程的监控
                                    close(users[del_user].pipefd[0]);
                                    users[del_user] = users[--user_count];
                                    sub_process[users[del_user].pid] = del_user;
                                }
                                if (terminate && user_count == 0) {
                                    stop_server = true;
                                }
                                break;
                            case SIGTERM:
                            case SIGINT:
                                printf("Kill at child now\n");
                                if (user_count == 0) {
                                    stop_server = true;
                                    break;
                                }
                                for (int i = 0; i < user_count; ++i) {
                                    int pid = users[i].pid;
                                    kill(pid, SIGTERM);
                                }
                                terminate = true;
                                break;
                            default:
                                break;
                        }
                    }
                }
            } else if (events[i].flags & EV_ENABLE) { // TODO
                int child = 0;
                ret = recv(sockfd, (char *)&child, sizeof(child), 0);
                // printf("Read data from child across PIPE\n");
                if (ret == -1) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    for (int j = 0; j < user_count; ++j) {
                        if (users[j].pipefd[0] != sockfd) {
                            printf("Send data to child across PIPE\n");
                            send(users[j].pipefd[0], (char *)&child, sizeof(child), 0);
                        }
                    }
                }
            }
        }
    }
    del_resource();
    return EXIT_SUCCESS;
}
