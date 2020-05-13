// Name   : server.hpp
// Author : Modnar
// Date   : 2020/04/12
// Copyright (c) 2020 Modnar. All rights reserved.

#ifndef _SERVER_HPP
#define _SERVER_HPP

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

extern const int USER_LIMIT;
extern const int HISTORY_MSG_NUM;
extern const int USER_INFO_SIZE;
extern const int MESSAGE_SIZE;
extern const int BUFFER_SIZE;
extern const int FD_LIMIT;
extern const int EVENT_LIMIT;

extern const char *COLOR_CLEAN;
extern const char *COLOR_BLUE;
extern const char *COLOR_GREEN;
extern const char *COLOR_RED;
extern const char *COLOR_WHITE;
extern const char *COLOR_YELLOW;

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

extern char *buffer;            /* 用于缓存服务器状态信息/处理用户发送来的信息缓冲区 */
extern char *historyMsgBuffer;  /* 服务器上记录的用户历史消息缓冲区 */
extern client_data *users;      /* 用于记录用户数据 */
extern int user_count;          /* 用于记录在线用户数 */
extern message_data *messages;  /* 用于记录存储历史消息 */
extern int msg_count;           /* 用于记录当前消息区消息总数 */
extern int curr_msg_pos;        /* 用于记录当前消息存应储于历史消息区的位置 */
extern bool modified;           /* 用于优化，标识服务器数据内容(messages)是否被修改 */

// int shmfd;
// const char *shm_name = "/my_shm";

#endif /* _SERVER_HPP */
