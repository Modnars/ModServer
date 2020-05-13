// Name   : command.cpp
// Author : Modnar
// Date   : 2020/05/13
// Copyright (c) 2020 Modnar. All rights reserved.

#include "command.hpp"

int parse_command(const char *cmdline);
void process_history_message();

/**
 * 解析命令
 *     根据传入的命令(包含'$'提示符)，判断具体的命令类型，并将类型以数值码的形式返回。
 */
int parse_command(const char *cmdline) {
    if (strlen(cmdline) > 2) {
        cmdline += 2; // 向前偏移两个字符，以越过'$'和' '
        if (!strncmp(cmdline, "history", strlen("history"))) {
            return 1;
        } else if (!strncmp(cmdline, "push", strlen("push"))) {
            return 2;
        } else if (!strncmp(cmdline, "pull", strlen("pull"))) {
            return 3;
        } else if (!strncmp(cmdline, "info", strlen("info"))) {
            return 4;
        } else {
            return 0;
        }
    }
    return -1;
}

/**
 * 格式化处理聊天室服务器历史消息
 *     将各个用户的消息格式化整理，存储到historyMsgBuffer。调用此函数后，
 * historyMsgBuffer存储内容为格式化的历史消息数据，此时的buffer可以直接被发送给
 * 客户端。
 */
void process_history_message() {
    if (modified) {
        int pos = (msg_count < HISTORY_MSG_NUM ? 0 : curr_msg_pos);
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
