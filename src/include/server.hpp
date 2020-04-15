// Name   : server.hpp
// Author : Modnar
// Date   : 2020/04/12

#ifndef _SERVER_HPP
#define _SERVER_HPP

#include <string>

extern const int USER_LIMIT;
extern const int HISTORY_MSG_NUM;
extern const int USER_INFO_SIZE;
extern const int MESSAGE_SIZE;
extern const int BUFFER_SIZE;
extern const int FD_LIMIT;
extern const int EVENT_LIMIT;

struct client_data {
    client_data() : uid(-1), clifd(-1) { }

    int uid;                /* 记录用户注册聊天时的ID */
    int clifd;              /* 记录用户socket文件描述符 */
    sockaddr_in address;    /* 客户端socket地址 */
    struct tm loginTime;    /* 登录时间 */
};

struct message_data {
    message_data() : uid(-1) { message = new char[MESSAGE_SIZE]; }
    ~message_data() { delete [] message; }

    int uid;
    char *message;
    struct tm time;
};

#endif /* _SERVER_HPP */
