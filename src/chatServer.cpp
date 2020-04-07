// Name   : server.cpp
// Author : Modnar
// Date   : 2020/04/07

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

#define USER_LIMIT          10
#define HISTORY_MSG_SIZE    10
#define TIME_STR_SIZE       30
#define BUFFER_SIZE         256
#define FD_LIMIT            65535
#define MAX_EVENT_SIZE      1024

#define COLOR_CLEAN         "\e[0m"
#define COLOR_BLUE          "\e[0;34m"
#define COLOR_GREEN         "\e[0;32m"
#define COLOR_RED           "\e[0;31m"
#define COLOR_WHITE         "\e[1;37m"
#define COLOR_YELLOW        "\e[1;33m"

// 实现目标功能，记录的用户数据
struct client_data {
    client_data() : idx(-1), clifd(-1), currpos(-1), hislen(0) {
        message = new char[HISTORY_MSG_SIZE*BUFFER_SIZE];
        timestamps = new struct tm[HISTORY_MSG_SIZE];
    }
    ~client_data() { 
        delete [] message;
        delete [] timestamps;
    }

    sockaddr_in address;    /* 客户端socket地址 */
    int idx;                /* 用于记录用户注册聊天时的id */
    int clifd;              /* socket文件描述符 */
    int currpos;            /* 标记用户当前状态(历史消息/时间戳) */
    int hislen;             /* 记录历史消息总条数，最大值为HISTORY_MSG_SIZE */
    char *message;          /* 历史消息 */
    struct tm loginTime;    /* 登录时间 */
    struct tm *timestamps;  /* 时间戳 */
};

time_t currtime;            // 计算当前时间
struct tm *ptm;             // 格式化存储时间
int user_count = 0;         // 用户总数, 可用于server查询命令
client_data *users = NULL;  // 用户数据存储区域

// 共享内存相关全局变量
static const char *shm_name = "/my_shm";
int shmfd;
char *share_mem = NULL;

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

/**
 * 处理用户idx历史消息
 *     将用户历史消息缓存到share_mem
 */
void execute_history_msg(int idx) {
    int hislen = users[idx].hislen, pos;
    bzero(share_mem, HISTORY_MSG_SIZE*(TIME_STR_SIZE+BUFFER_SIZE));
    if (hislen < HISTORY_MSG_SIZE) {
        pos = 0;
    } else {
        pos = (users[idx].currpos + 1) % HISTORY_MSG_SIZE;
    }
    for (int i = 0; i < hislen; ++i) {
        snprintf(share_mem+i*(TIME_STR_SIZE+BUFFER_SIZE), TIME_STR_SIZE,
                "[%d/%02d/%02d %02d:%02d:%02d]", 
                users[idx].timestamps[pos].tm_year+1900, users[idx].timestamps[pos].tm_mon+1,
                users[idx].timestamps[pos].tm_mday, users[idx].timestamps[pos].tm_hour,
                users[idx].timestamps[pos].tm_min, users[idx].timestamps[pos].tm_sec);
        strncpy(share_mem+i*(TIME_STR_SIZE+BUFFER_SIZE)+TIME_STR_SIZE, users[idx].message+pos*BUFFER_SIZE,
                BUFFER_SIZE);
        pos = (pos + 1) % HISTORY_MSG_SIZE;
    }
}

// 在服务器停止关闭时回收资源
void del_resource() {
    delete [] users;
    shm_unlink(shm_name);
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
    check((listenfd != -1), "[FAILED]: AT CREATE SOCKET", "[OK]: AT CREATE SOCKET");

    // 将server地址与socket绑定
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    check((ret != -1), "[FAILED]: AT CREATE BIND", "[OK]: AT CREATE BIND");

    // 进行监听
    ret = listen(listenfd, USER_LIMIT);
    check((ret != -1), "[FAILED]: AT CREATE LISTEN", "[OK]: AT CREATE LISTEN");

    // 设定`超时时长`，若服务器在超时时长内未监听到任何消息，则关闭服务器
    struct timespec timeout = {60, 0};
    user_count = 0;
    users = new client_data[USER_LIMIT];

    // 下面这两行代码用于回收因服务器异常退出而导致的`共享内存`未正常回收的问题
    //     这里需要展开说一下，共享内存是一种典型的解决多进程数据共享的方式，实际应用中，可能
    // 需要多个进程同时访问一块共享内存，因而务必保证每个程序都能按正常的逻辑进行退出以保证共享
    // 内存的`引用次数`正常，否则容易导致共享内存`常驻`的问题。若这块共享内存已存在，则下面的
    // 重新规划该空间大小的代码就会报错(ftruncate函数)，因而为了后续代码正常执行，请确保共享内存
    // 状态正常(对本程序来讲，就是要每次开始时申请，每次退出时释放，这是和服务器数量相关的)。
    // shm_unlink(shm_name);
    // return EXIT_SUCCESS;
    // 创建共享内存来保存所有需要处理的数据
    shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    check((shmfd != -1), "[FAILED]: AT CREATE SHARE_MEMORY", "[OK]: AT CREATE SHARE_MEMORY");
    // 为这块共享内存分配足够大的空间
    ret = ftruncate(shmfd, HISTORY_MSG_SIZE*(TIME_STR_SIZE+BUFFER_SIZE));
    check((ret != -1), "[FAILED]: AT REQUEST MEMORY", "[OK]: AT REQUEST MEMORY");
    // 申请映射到该共享内存上
    share_mem = (char *)mmap(NULL, HISTORY_MSG_SIZE*(TIME_STR_SIZE+BUFFER_SIZE), 
            PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    check((share_mem != MAP_FAILED), "[FAILED]: AT MMAP MEMORY", "[OK]: AT MMAP MEMORY");
    close(shmfd);

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
            printf("%s[FAILED]: AT KEVENT%s\n", COLOR_RED, COLOR_CLEAN); 
            break;
        } else if (number == 0) { // 超时: 这里定义超时动作为关闭服务器
            printf("%s[TIME_OUT]: CLOSE THE SERVER.%s\n", COLOR_RED, COLOR_CLEAN);
            stop_server = true;
        } else { // 与epoll相似，直接对number进行遍历判断即可
            for (int i = 0; i < number; ++i) {
                int sockfd = events[i].ident;
                currtime = time(NULL);                  // 获取处理事件的时刻
                ptm = localtime(&currtime);  // 格式化时间，得到时间存储对象指针ptm
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
                            users[idx].idx = idx;
                            users[idx].clifd = connfd;
                            users[idx].address = cli_addr;
                            users[idx].loginTime = *ptm; // 记录用户数据: 登录时间
                            ++user_count;
                            // server日志信息: 打印用户加入信息及时间信息
                            printf("%s[SUCCESS(%d/%02d/%02d %02d:%02d:%02d)]: CLIENT#%d(%s:%d) JOINED%s\n", 
                                    COLOR_GREEN, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour,
                                    ptm->tm_min, ptm->tm_sec, users[idx].idx, inet_ntoa(cli_addr.sin_addr), 
                                    ntohs(cli_addr.sin_port), COLOR_CLEAN);
                            bzero(share_mem, BUFFER_SIZE);
                            snprintf(share_mem, BUFFER_SIZE, "[WELCOME(%d/%02d/%02d %02d:%02d:%02d)]: Your ID is %d.",
                                    users[idx].loginTime.tm_year+1900, users[idx].loginTime.tm_mon+1,
                                    users[idx].loginTime.tm_mday, users[idx].loginTime.tm_hour,
                                    users[idx].loginTime.tm_min, users[idx].loginTime.tm_sec, idx);
                            send(connfd, share_mem, BUFFER_SIZE, 0);
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
                    bzero(share_mem, BUFFER_SIZE);
                    ret = recv(sockfd, share_mem, BUFFER_SIZE, 0);
                    if (ret == 0) { // 用户退出客户端, 关闭连接，清除用户数据
                        printf("%s[EXIT_CHAT(%d/%02d/%02d %02d:%02d:%02d)]: CLIENT#%d EXIT THE CHAT%s\n", 
                                COLOR_YELLOW, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, 
                                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, idx, COLOR_CLEAN);
                        EV_SET(&changes, sockfd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                        kevent(kq, &changes, 1, NULL, 0, NULL);
                        close(sockfd);
                        users[idx].idx = -1;        // 将users中该位置标识为无用户
                        users[idx].currpos = -1;    // 重置该位置的历史消息/时间戳存储位置
                        users[idx].hislen = 0;      // 重置该用户的历史记录数
                        --user_count;
                    } else if (ret > 0) { // 用户发来消息
                        if (!strncmp(share_mem, "history", strlen("history"))) {
                            execute_history_msg(idx);
                            printf("%s[COMMAND: HISTORY] CLIENT#%d %d/%02d/%02d %02d:%02d:%02d%s\n",
                                    COLOR_WHITE, idx, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, 
                                    ptm->tm_hour, ptm->tm_min, ptm->tm_sec, COLOR_CLEAN);
                            send(sockfd, share_mem, HISTORY_MSG_SIZE*(TIME_STR_SIZE+BUFFER_SIZE), 0);
                            continue;
                        }
                        // 更新用户历史消息/时间戳存储位置
                        users[idx].currpos = (users[idx].currpos + 1) % HISTORY_MSG_SIZE;
                        if (users[idx].hislen < HISTORY_MSG_SIZE) ++users[idx].hislen;  // 更新用户拥有记录数
                        users[idx].timestamps[users[idx].currpos] = *ptm; // 记录用户发送消息时间戳
                        strncpy(users[idx].message+users[idx].currpos*BUFFER_SIZE, share_mem, BUFFER_SIZE); // TODO splice
                        printf("%s[RECV(%d/%02d/%02d %02d:%02d:%02d/%2dB)] CLIENT#%d:'%s'%s\n", 
                                COLOR_BLUE, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, 
                                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, ret, idx, share_mem, COLOR_CLEAN);
                        bzero(share_mem, BUFFER_SIZE);
                        snprintf(share_mem, BUFFER_SIZE, "[OK] Time: %d/%02d/%02d %02d:%02d:%02d",
                                ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, 
                                ptm->tm_min, ptm->tm_sec);
                        send(sockfd, share_mem, BUFFER_SIZE, 0);
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
