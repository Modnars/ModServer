/**
 * network.h
 *     下面是一些Linux下网络编程可能用到的头文件、数据结构定义以及处理函数，供学习以及
 * 记忆性复习使用。
 */
// Author : Modnar
// Date   : 2020/04/01

/**
 * socket地址api
 */
// Linux提供的4个函数以完成主机字节序和网络字节序之间的转换
#include <netinet/in.h>

unsigned long int htonl(unsigned long int hostlong);
unsigned short int htons(unsigned short int hostshort);
unsigned long int ntohl(unsigned long int netlong);
unsigned short int ntohs(unsigned short int netshort);

#include <bits/socket.h>

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
}

struct sockaddr_storage {
    sa_family_t sa_family;
    unsigned long int __ss_align;
    char __ss_padding[128-sizeof(__ss_align)];
}

// 5.1.3 专用socket地址
#include <sys/un.h>
struct sockaddr_un {
    sa_family_t sin_family; /* 地址族: AF_UNIX */
    char sun_path[108];     /* 文件路径名 */
};

struct sockaddr_in {
    sa_family_t sin_family;     /* 地址族: AF_INET */
    u_int16_t sin_port;         /* 端口号，要用网络字节序表示 */
    struct in_addr sin_addr;    /* IPv4地址结构体，见下面 */
};

struct in_addr {
    u_int32_t s_addr;           /* IPv4地址，要用网络字节序表示 */
};

struct sockaddr_in6 {
    sa_family_t sin6_family;    /* 地址族: AF_INET6 */
    u_int16_t sin6_port;        /* 端口号，要用网络字节序表示 */
    u_int32_t sin6_flowinfo;    /* 流信息，应设置为0 */
    struct in6_addr sin6_addr;  /* IPv6地址结构体，见下面 */
    u_int32_t sin6_scope_id;    /* scope ID，尚处于实验阶段 */
};

struct in6_addr {
    unsigned char sa_addr[16];  /* IPv6地址，要用网络字节序表示 */
};

// 5.1.4 IP地址转换函数
#include <arpa/inet.h>
// inet_addr将采用点分十进制字符串表示的IPv4地址转换为网络字节序整数表示的IPv4地址
// 失败时返回INADDR_NONE
in_addr_t inet_addr(const char *strptr);
// 功能与inet_addr相同，但将转化结果存储于inp指向的地质结构中。成功返回1，失败返回0
int inet_aton(const char *cp, struct in_addr *inp);
// inet_ntoa函数将用网络字节序整数表示的IPv4地址转化为用点分十进制字符串表示的IPv4
// 地址。但需要注意的是，该函数内部采用一个静态变量存储转化结果，函数的返回值指向该
// 静态内存，因此inet_ntoa是不可重入的。
char *inet_ntoa(struct in_addr in);

// 下面这对更新的函数也可以完成上述三个函数的功能，且可以同时适用于IPv4和IPv6地址
#include <arpa/inet.h>
// inet_pton: src: IP地址(点分十进制) dst: 转换结果存储内存地址 af: 参数指定地址族
// 成功时返回1，失败返回0并设置errno
int inet_pton(int af, const char *src, void *dst);
// 参数函数同inet_pton相同，cnt指定目标存储单元的大小，下面的两个宏可以帮助指定大小
// 调用成功时返回目标存储单元的地址，失败则返回NULL并设置errno
const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt);

#include <netinet/in.h>
#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

/**
 * 5.2 创建socket
 */
// 创建一个socket
#include <sys/types.h>
#include <sys/socket.h>
// domain: 底层协议族(PF_INET:IPv4, PF_INET6:IPv6, PF_UNIX:UNIX本地域协议族)
// type: 服务类型(SOCK_STREAM: 流服务(TCP), SOCK_UGRAM: 数据报服务(UDP))
// protocol: 在前两个参数构成的协议集合下，再选择一个具体的协议。不过这个值通常都是
//     唯一的(前两个参数已经完全决定了它的值)。几乎在所有情况下，我们都应该把它设置
//     为0，表示使用默认协议。
// 调用成功时返回一个socket文件描述符，失败时返回-1并设置errno
int socket(int domain, int type, int protocol);

/**
 * 5.3 命名socket
 */
#include <sys/types.h>
#include <sys/socket.h>
// 调用成功时返回0，失败时返回-1并设置errno。两种常见的errno是EACCES和EADDRINUSE
int bind(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen);

/**
 * 5.4 监听socket
 */
#include <sys/socket.h>
// sockfd: 被监听的socket
// backlog: 提示内核监听队列的最大长度
// 调用成功时返回0，失败时返回-1并设置errno
int listen(int sockfd, int backlog);

/**
 * 5.5 接受连接
 */
#include <sys/types.h>
#include <sys/socket.h>
// sockfd: 执行过listen系统调用的监听socket(处于LISTEN而非ESTABLISHED状态)
// addr: 获取被接受连接的远端socket地址，该地址长度由addrlen参数给出
// addrlen: 用于给出addr的地址长度
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

/**
 * 5.6 发起连接
 */
#include <sys/types.h>
#include <sys/socket.h>
// sockfd: 由socket系统调用返回一个socket
// serv_addr: 服务器监听的socket地址
// addrlen: 指定这个地址的长度
// 调用成功时返回0。一旦成功建立连接，sockfd就唯一地标识了这个连接，客户端就可以通过
// 读写sockfd来与服务器通信。connet失败返回-1并设置errno。两种常见错误: ECONNREFUSED
// 和ETIMEDOUT
int connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen);

/**
 * 5.7 关闭连接
 */
#include <unistd.h>
// fd: 待关闭的socket。close系统调用并非立即关闭连接，而是将fd引用计数减一。
int close(int fd);

#include <sys/socket.h>
//     如果无论如何都要立即终止连接，可以使用shutdown系统调用(相对于close来说，它是专
// 门为网络编程设计的。
// sockfd: 待关闭的socket
// howto: 决定shutdown的行为(SHUT_RD, SHUT_WR, SHUT_RDWR)
int shutdown(int sockfd, int howto);

/**
 * 5.8 数据读写
 */
// 5.8.1 TCP数据读写
#include <sys/types.h>
#include <sys/socket.h>
// 读取sockfd上的数据
// buf: 读缓冲区的位置  len: 读缓冲区的大小  flags: 通常设置为0即可
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);

// 5.8.2 UDP数据读写
#include <sys/types.h>
#include <sys/socket.h>
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, 
        struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, 
        const struct sockaddr *dest_addr, socklen_t addrlen);

// 5.8.3 通用数据读写函数
#include <sys/socket.h>
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
ssize_t sendmsg(int sockfd, struct msghdr *msg, int flags);

// msghdr结构体定义
struct msghdr {
    void *msg_name;             /* socket地址 */
    socklen_t msg_namelen;      /* socket地址的长度 */
    struct iovec *msg_iov;      /* 分散的内存块，见下文 */
    int msg_iovlen;             /* 分散的内存块数量 */
    void *msg_control;          /* 指向辅助数据的起始位置 */
    socklen_t msg_controllen;   /* 辅助数据的大小 */
    int msg_flags;              /* 复制函数中的flags参数，并在调用过程中更新 */
};

struct iovec {
    void *iov_base; /* 内存起始位置 */
    size_t iov_len; /* 这块内存的长度 */
};

/**
 * 5.9 带外标记
 */
#include <sys/socket.h>
//     判断sockfd是否处于带外标记，即下一个被读取到的数据是否是带外数据。若是，返回1，
// 此时可以利用带MSG_OOB标志的recv调用来接收带外数据；若不是，则返回0。
int sockatmark(int sockfd);

/**
 * 5.10 地址信息函数
 */
#include <sys/socket.h>
//     获取sockfd对应的本端socket地址，并将其存储于address指定的内存中，长度存储于
// address_len参数指向的变量中，若实际socket地址长度大于address所指内存区的大小，那么
// 该socket地址将被截断
//     成功时返回0，失败时返回-1并设置errno
int getsockname(int sockfd, struct sockaddr *address, socklen_t *address_len);
//     获取sockfd对应的远端socket地址
int getpeername(int sockfd, struct sockaddr *address, socklen_t *address_len);

/**
 * 5.11 socket选项
 */

/**
 * 5.12 网络信息API
 */
