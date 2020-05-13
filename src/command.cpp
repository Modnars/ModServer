// Name   : server.cpp
// Author : Modnar
// Date   : 2020/04/16
// Copyright (c) 2020 Modnar. All rights reserved.

#include "command.hpp"

void process_history_message();

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
            // snprintf(buffer+i*BUFFER_SIZE, BUFFER_SIZE, 
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
