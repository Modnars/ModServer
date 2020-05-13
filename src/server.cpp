// Name   : server.cpp
// Author : Modnar
// Date   : 2020/04/16

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
#include <time.h>
#include <unistd.h>

#include "server.hpp"

const int USER_LIMIT        = 10;
const int HISTORY_MSG_NUM   = 10;
const int USER_INFO_SIZE    = 64;
const int MESSAGE_SIZE      = 256;
const int BUFFER_SIZE       = USER_INFO_SIZE + MESSAGE_SIZE;
const int FD_LIMIT          = 65535;
const int EVENT_LIMIT       = 1024;

// C++的文件作用域(相当于C的static)
namespace {
    const char *COLOR_CLEAN  = "\e[0m";
    const char *COLOR_BLUE   = "\e[0;34m";
    const char *COLOR_GREEN  = "\e[0;32m";
    const char *COLOR_RED    = "\e[0;31m";
    const char *COLOR_WHITE  = "\e[1;37m";
    const char *COLOR_YELLOW = "\e[1;33m";

    time_t currtime; // 用于获取当前时间
    struct tm *ptm;  // 格式化记录当前时间

    char *buffer = nullptr;             /* 用于缓存服务器状态信息/处理用户发送来的信息缓冲区 */
    // int shmfd;
    // const char *shm_name = "/my_shm";
    char *historyMsgBuffer = nullptr;   /* 服务器上记录的用户历史消息缓冲区 */
    bool modified;                      /* 用于优化查询效率，避免server总是刷新historyMsgBuffer */
    client_data *users = nullptr;       /* 用于记录用户数据 */
    int user_count;                     /* 用于记录在线用户数 */
    message_data *messages = nullptr;   /* 用于记录存储历史消息 */
    int curr_msg_pos;                   /* 用于记录当前消息存应储于历史消息区的位置 */
    int msg_count;                      /* 用于记录当前历史消息区消息总数 */
}

// 将文件描述符fd设置为非阻塞，以便于IO复用
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * 查找用户数据
 *     支持按ID(用户标识符, 由server根据运行状态自动分配)、按connfd(用户连接描述符)
 * 索引查找。若找到目标key，则返回该用户数据的uid，否则返回-1。
 * @param: key: 查询索引
 * @param: byConnfd: 查询条件选项，默认按idx进行搜索
 */
int search_user(int key, bool byConnfd = false) {
    if (byConnfd) {
        for (int i = 0; i < USER_LIMIT; ++i)
            if (users[i].clifd == key)
                return i;
    } else {
        for (int i = 0; i < USER_LIMIT; ++i)
            if (users[i].uid == key)
                return i;
    }
    return -1;
}

void add_user_message(int uid, char *message, struct tm *currTime) {
    modified = true;
    messages[curr_msg_pos].uid = uid;
    bzero(messages[curr_msg_pos].message, MESSAGE_SIZE);
    strncpy(messages[curr_msg_pos].message, message, MESSAGE_SIZE-1);
    messages[curr_msg_pos].time = *currTime;
    if (msg_count < HISTORY_MSG_NUM) ++msg_count;
    curr_msg_pos = (curr_msg_pos + 1) % HISTORY_MSG_NUM;
}

void execute_history_message() {
    if (modified) {
        int pos;
        if (msg_count < HISTORY_MSG_NUM) {
            pos = 0;
        } else {
            pos = curr_msg_pos;
        }
        for (int i = 0; i != msg_count; ++i) {
            snprintf(historyMsgBuffer+i*BUFFER_SIZE, BUFFER_SIZE, 
                    "[%d/%02d/%02d %02d:%02d:%02d] @%d: %s", 
                    messages[(pos+i)%HISTORY_MSG_NUM].time.tm_year+1900, 
                    messages[(pos+i)%HISTORY_MSG_NUM].time.tm_mon+1, 
                    messages[(pos+i)%HISTORY_MSG_NUM].time.tm_mday, 
                    messages[(pos+i)%HISTORY_MSG_NUM].time.tm_hour, 
                    messages[(pos+i)%HISTORY_MSG_NUM].time.tm_min, 
                    messages[(pos+i)%HISTORY_MSG_NUM].time.tm_sec, 
                    messages[(pos+i)%HISTORY_MSG_NUM].uid,
                    messages[(pos+i)%HISTORY_MSG_NUM].message);
        }
        modified = false;
    }
}

void release_resource() {
    delete ptm;
    delete [] buffer;
    delete [] users;
    delete [] messages;
}

/**
 * 检查fd是否是一个合法文件描述符
 * @param: res: 判断结果(若为true，表示成功；否则表示失败)
 * @param: succMsg: 日志信息(创建成功)
 * @param: errMsg: 日志信息(创建失败)
 * 若fd非法，打印失败日志并终止当前进程; 否则打印创建成功日志信息
 */
void check(bool res, const char *succMsg, const char *errMsg) {
    if (res) {
        printf("%s\n", succMsg);
    } else {
        perror(errMsg);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    // 处理程序传入参数
    if (argc <= 2) {
        printf("%susage: %s ip_address port_number%s\n", COLOR_RED, basename(argv[0]), COLOR_CLEAN);
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
    check((listenfd != -1), "[OK]: AT CREATE SOCKET", "[FAILED]: AT CREATE SOCKET");

    // 将server地址与socket绑定
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    check((ret != -1), "[OK]: AT CREATE BIND", "[FAILED]: AT CREATE BIND");

    // 进行监听
    ret = listen(listenfd, USER_LIMIT);
    check((ret != -1), "[OK]: AT CREATE LISTEN", "[FAILED]: AT CREATE LISTEN");

    // 设定`超时时长`，若服务器在超时时长内未监听到任何消息，则关闭服务器
    struct timespec timeout = {60, 0};
    ptm = new struct tm;
    buffer = new char[MESSAGE_SIZE];
    users = new client_data[USER_LIMIT];
    user_count = 0;
    messages = new message_data[HISTORY_MSG_NUM];
    msg_count = 0;
    curr_msg_pos = 0;
    historyMsgBuffer = new char[HISTORY_MSG_NUM*BUFFER_SIZE];

    // 下面这两行代码用于回收因服务器异常退出而导致的`共享内存`未正常回收的问题
    // shm_unlink(shm_name);
    // return EXIT_SUCCESS;
    // 创建共享内存来保存所有需要处理的数据
    // shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    // check((shmfd != -1), "[OK]: AT CREATE SHARE_MEMORY", "[FAILED]: AT CREATE SHARE_MEMORY");
    // 为这块共享内存分配足够大的空间
    // ret = ftruncate(shmfd, HISTORY_MSG_NUM*(USER_INFO_SIZE+MESSAGE_SIZE));
    // check((ret != -1), "[OK]: AT REQUEST MEMORY", "[FAILED]: AT REQUEST MEMORY");
    // 申请映射到该共享内存上
    // share_mem = (char *)mmap(NULL, HISTORY_MSG_NUM*(USER_INFO_SIZE+MESSAGE_SIZE), 
    //         PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    // check((share_mem != MAP_FAILED), "[OK]: AT MMAP MEMORY", "[FAILED]: AT MMAP MEMORY");
    // close(shmfd);

    // 获得kqueue
    int kq = kqueue();
    check((kq != -1), "[OK]: AT CREATE KQUEUE", "[FAILED]: AT CREATE KQUEUE");
    struct kevent changes, events[EVENT_LIMIT];
    EV_SET(&changes, listenfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &changes, 1, NULL, 0, NULL);

    // 标识服务器运行状态
    bool stop_server = false;

    while (!stop_server) {
        int number = kevent(kq, NULL, 0, events, EVENT_LIMIT, &timeout);
        if ((number < 0) && (errno != EINTR)) {
            printf("%s[FAILED]: AT KEVENT%s\n", COLOR_RED, COLOR_CLEAN); 
            break;
        } else if (number == 0) { // 超时: 这里定义超时动作为关闭服务器
            printf("%s[TIME_OUT]: THE SERVER IS CLOSED.%s\n", COLOR_RED, COLOR_CLEAN);
            stop_server = true;
        } else { // 与epoll相似，直接对number进行遍历判断即可
            for (int i = 0; i < number; ++i) {
                int sockfd = events[i].ident;
                currtime = time(NULL);                  // 获取处理事件的时刻
                localtime_r(&currtime, ptm);  // 格式化时间，得到时间存储对象指针ptm
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
                            users[idx].uid = idx;
                            users[idx].clifd = connfd;
                            users[idx].address = cli_addr;
                            users[idx].loginTime = *ptm; // 记录用户数据: 登录时间
                            ++user_count;
                            // server日志信息: 打印用户加入信息及时间信息
                            printf("%s[SUCCESS(%d/%02d/%02d %02d:%02d:%02d)]: CLIENT#%d(%s:%d) JOINED%s\n", 
                                    COLOR_GREEN, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour,
                                    ptm->tm_min, ptm->tm_sec, users[idx].uid, inet_ntoa(cli_addr.sin_addr), 
                                    ntohs(cli_addr.sin_port), COLOR_CLEAN);
                            bzero(buffer, MESSAGE_SIZE);
                            snprintf(buffer, MESSAGE_SIZE, 
                                    "[WELCOME(%d/%02d/%02d %02d:%02d:%02d)]: Your ID is %d.",
                                    users[idx].loginTime.tm_year+1900, users[idx].loginTime.tm_mon+1,
                                    users[idx].loginTime.tm_mday, users[idx].loginTime.tm_hour,
                                    users[idx].loginTime.tm_min, users[idx].loginTime.tm_sec, idx);
                            send(connfd, buffer, MESSAGE_SIZE, 0);
                        } else { // 反馈无法增添用户，并关闭该用户请求连接
                            const char *info = "[FAILED]: TOO MUCH USERS!";
                            printf("%s%s%s\n", COLOR_RED, info, COLOR_CLEAN);
                            send(connfd, info, strlen(info), 0);
                            close(connfd);
                        }
                    } else { // accept返回值小于等于零
                        printf("%s[ErrNo:%d]: %s%s\n", COLOR_RED, errno, strerror(errno), COLOR_CLEAN);
                        continue;
                    }
                } else { // 接收用户信息/状态
                    int idx = search_user(sockfd, true);
                    bzero(buffer, MESSAGE_SIZE);
                    ret = recv(sockfd, buffer, MESSAGE_SIZE, 0);
                    if (ret == 0) { // 用户退出客户端, 关闭连接，清除用户数据
                        printf("%s[EXIT(%d/%02d/%02d %02d:%02d:%02d)]: CLIENT#%d EXIT THE CHATROOM%s\n", 
                                COLOR_YELLOW, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, 
                                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, idx, COLOR_CLEAN);
                        EV_SET(&changes, sockfd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                        kevent(kq, &changes, 1, NULL, 0, NULL);
                        close(sockfd);
                        users[idx].uid = -1;        // 将users中该位置标识为无用户
                        users[idx].clifd = -1;
                        --user_count;
                    } else if (ret > 0) { // 用户发来消息
                        if (!strncmp(buffer, "history", strlen("history"))) {
                            execute_history_message();
                            printf("%s[COMMAND: HISTORY] CLIENT#%d %d/%02d/%02d %02d:%02d:%02d%s\n",
                                    COLOR_WHITE, idx, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, 
                                    ptm->tm_hour, ptm->tm_min, ptm->tm_sec, COLOR_CLEAN);
                            send(sockfd, historyMsgBuffer, HISTORY_MSG_NUM*BUFFER_SIZE, 0);
                            continue;
                        }
                        // 更新用户历史消息/时间戳存储位置
                        printf("%s[RECV(%d/%02d/%02d %02d:%02d:%02d/%2dB)] CLIENT@%d:'%s'%s\n", 
                                COLOR_BLUE, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, 
                                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, ret, idx, buffer, COLOR_CLEAN);
                        add_user_message(users[idx].uid, buffer, ptm);
                        bzero(buffer, MESSAGE_SIZE);
                        snprintf(buffer, MESSAGE_SIZE, "[OK] Time: %d/%02d/%02d %02d:%02d:%02d",
                                ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, 
                                ptm->tm_min, ptm->tm_sec);
                        send(sockfd, buffer, MESSAGE_SIZE, 0);
                    } else { // 其他问题
                        printf("[CLIENT#%d]: WRONG STATUS\n", idx);
                    }
                }
            }
        }
    }
    release_resource();
    return EXIT_SUCCESS;
}
