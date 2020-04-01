/**
 * network.h
 *     下面是一些Linux下网络编程可能用到的头文件、数据结构定义以及处理函数，供学习以及
 * 记忆性复习使用。
 */
// Author : Modnar
// Date   : 2020/04/01

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
// 成功时返回目标存储单元的地址，失败则返回NULL并设置errno
const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt);

#include <netinet/in.h>
#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46
