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
#define HISTORY_MSG_SIZE    20
#define BUFFER_SIZE         128
#define FD_LIMIT            65535
#define MAX_EVENT_SIZE      1024

// 实现目标功能，记录的用户数据
struct client_data {
    client_data() : idx(-1), clifd(-1) {
        message = new char[HISTORY_MSG_SIZE*BUFFER_SIZE];
    }
    ~client_data() { delete [] message; }

    sockaddr_in address;    /* 客户端socket地址 */
    int idx;                /* 用于记录用户注册聊天时的id */
    int clifd;              /* socket文件描述符 */
    int tempstamp;          /* 时间戳 */
    char *message;          /* 历史消息 */
};

int user_count = 0;         // 用户总数, 可用于server查询命令
client_data *users = NULL;  // 用户数据存储区域
char *buffer = NULL;        // server处理数据缓冲区

// 将文件描述符fd设置为非阻塞，以便于IO复用
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * 查找用户数据
 *     支持按idx(用户标识符, 由server根据运行状态自动分配)、按connfd(用户连接描述符)
 * 索引查找。若找到目标key，则返回该用户数据的idx，否则返回-1。
 *   key: 查询索引
 *   byConnfd: 查询条件选项，默认按idx进行搜索
 */
int search_user(int key, bool byConnfd = false) {
    if (byConnfd) {
        for (int i = 0; i < USER_LIMIT; ++i)
            if (users[i].clifd == key)
                return i;
    } else {
        for (int i = 0; i < USER_LIMIT; ++i)
            if (users[i].idx == key)
                return i;
    }
    return -1;
}

// 在服务器停止关闭时回收资源
void del_resource() {
    delete [] users;
    delete [] buffer;
}

/**
 * 检查fd是否是一个合法文件描述符
 *   res: 判断结果(若为true，表示成功；否则表示失败)
 *   errMsg: 日志信息(创建失败)
 *   successMsg: 日志信息(创建成功)
 * 若fd非法，打印失败日志并终止当前进程; 否则打印创建成功日志信息
 */
void check(bool res, const char *errMsg, const char *successMsg) {
    if (!res) {
        perror(errMsg);
        exit(EXIT_FAILURE);
    } else {
        printf("%s\n", successMsg);
    }
}

int main(int argc, char *argv[]) {
    // 处理程序传入参数
    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return EXIT_FAILURE;
    }
    // 转换并存储IP、PORT数据
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    // ret用于记录数据及判断程序运行状态是否异常，listenfd为创建的socket描述符
    int ret, listenfd;
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

    // 设定`超时时长`，若服务器在超时时长内未监听到任何消息，则关闭服务器
    struct timespec timeout = {120, 0};
    user_count = 0;
    users = new client_data[USER_LIMIT+1];
    buffer = new char[BUFFER_SIZE+1];

    // 获得kqueue
    int kq = kqueue();
    check((kq != -1), "[FAILED]: AT CREATE KQUEUE", "[OK]: AT CREATE KQUEUE");
    struct kevent changes, events[MAX_EVENT_SIZE];
    EV_SET(&changes, listenfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &changes, 1, NULL, 0, NULL);

    // 标识服务器运行状态
    bool stop_server = false;

    while (!stop_server) {
        int number = kevent(kq, NULL, 0, events, MAX_EVENT_SIZE, &timeout);
        if ((number < 0) && (errno != EINTR)) {
            printf("[FAILED]: AT KEVENT\n"); 
            break;
        } else if (number == 0) { // 超时: 这里定义超时动作为关闭服务器
            std::cout << "[TIME_OUT]: CLOSED THE SERVER." << std::endl;
            stop_server = true;
        } else { // 与epoll相似，直接对number进行遍历判断即可
            for (int i = 0; i < number; ++i) {
                int sockfd = events[i].ident;
                // 为客户端建立新的连接
                if (sockfd == listenfd) {
                    struct sockaddr_in cli_addr;
                    socklen_t cli_addrlen = sizeof(cli_addr);
                    int connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_addrlen);
                    if (connfd > 0) {
                        // 查找是否存在idx值为-1的位置，用于分配用户存储空间
                        int idx = search_user(-1);
                        if (idx >= 0) { // 仍有空间来存储该用户数据，将其加入并监听
                            setnonblocking(connfd); // 将connfd设置为非阻塞状态
                            // 监听该连接的输入
                            EV_SET(&changes, connfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                            // 放入事件队列
                            kevent(kq, &changes, 1, NULL, 0, NULL);
                            printf("[SUCCESS]: CLIENT#%d(%s:%d) JOINED\n", idx,
                                    inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                            users[idx].idx = idx;
                            users[idx].clifd = connfd;
                            users[idx].address = cli_addr;
                            ++user_count;
                        } else { // 反馈无法增添用户，并关闭该用户请求连接
                            const char *info = "[FAILED]: TOO MUCH USERS!";
                            printf("%s\n", info);
                            send(connfd, info, strlen(info), 0);
                            close(connfd);
                        }
                    } else { // accept返回值小于等于零
                        printf("[ErrNo:%d]: %s\n", errno, strerror(errno));
                        continue;
                    }
                } else { // 接收用户信息/状态
                    int idx = search_user(sockfd, true);
                    bzero(buffer, BUFFER_SIZE);
                    ret = recv(sockfd, buffer, BUFFER_SIZE, 0);
                    if (ret == 0) { // 用户退出客户端, 关闭连接，清除用户数据
                        printf("[CLIENT#%d]: EXIT THE CHATTING\n", idx);
                        EV_SET(&changes, sockfd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                        kevent(kq, &changes, 1, NULL, 0, NULL);
                        close(sockfd);
                        users[idx].idx = -1;
                        --user_count;
                    } else if (ret > 0) { // 用户发来消息
                        printf("[RECV(%dB)] CLIENT#%d: '%s'\n", ret, idx, buffer);
                    } else { // 其他问题
                        printf("[CLIENT#%d]: WRONG STATUS\n", idx);
                    }
                }
            }
        }
    }
    del_resource();
    return EXIT_SUCCESS;
}
