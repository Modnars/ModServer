#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/socket.h>
 
#include <sys/time.h>
#include <sys/types.h>
 
#include <unistd.h>
#include <fcntl.h>
 
#define BACKLOG 5           //完成三次握手但没有accept的队列长度
#define CONCURRENT_MAX 8    //应用层同时可以处理的连接
#define SERVER_PORT 9588
#define BUFFER_SIZE 1024
#define QUIT_CMD "quit"
 
int clients[CONCURRENT_MAX];
 
void check(int fd, const char *errorMsg, const char *sucMsg) {
    if (fd < 0) {
        perror(errorMsg);
        exit(1);
    } else {
        printf("%s", sucMsg);
    }
}
 
int main(int argc, char *argv[]) {
    char input_msg[BUFFER_SIZE];
    char recv_msg[BUFFER_SIZE];
    
    /* struct sockaddr_in {
     *  __uint8_t       sin_len;
     *  sa_family       sin_family;     //协议族，AF_xx
     *  in_port_t       sin_port;       //端口号，网络字节顺序
     *  struct in_addr  sin_addr;       //ip地址，4字节
     *  char            sin_zero[8];    //8字节，为了和sockaddr保持一样大小
     * }
     * int 为机器字长 2/4字节
     * short int 2字节
     * long int 4字节
     * long long 8字节
     */
    struct sockaddr_in addr4;
    memset(&addr4, 0, sizeof(addr4));
    addr4.sin_len = sizeof(struct sockaddr_in);
    addr4.sin_family = AF_INET;
    addr4.sin_addr.s_addr = inet_addr("192.168.0.103");
    addr4.sin_port = htons(SERVER_PORT);

    int server = socket(AF_INET, SOCK_STREAM, 0);
    check(server, "server create failed \n", "create good \n");
    
    int b = bind(server, (struct sockaddr *)&addr4, sizeof(struct sockaddr));
    check(b, "server bind failed \n", "bind good \n");
 
    int l = listen(server, BACKLOG);
    check(l, "server listen failed \n", "listen good \n");
    
    struct timespec timeout = {10, 0};
    
    int kq = kqueue();
    check(kq, "create queue failed \n", "kqueue good \n");

    struct kevent changes;
    EV_SET(&changes, STDIN_FILENO, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &changes, 1, NULL, 0, NULL);
    
    EV_SET(&changes, server, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &changes, 1, NULL, 0, NULL);
    
    struct kevent events[CONCURRENT_MAX + 2];
    
    while (1) {
        int ret = kevent(kq, NULL, 0, events, 10, &timeout);
        if (ret < 0) {
            printf("kevent error \n");
            continue;
            
        } else if (ret == 0) {
            printf("kevent timeout \n");
            continue;
            
        } else {
            printf("kevent good \n");
            
            for (int i = 0; i<ret; i++) {
                struct kevent current_event = events[i];
                
                //server input
                if (current_event.ident == STDIN_FILENO) {
                    bzero(input_msg, BUFFER_SIZE);
                    fgets(input_msg, BUFFER_SIZE, stdin);
                    
                    //quit
                    if (strncmp(input_msg, "quit", 4) == 0) {
                        exit(0);
                    }
                    
                    //broadcast
                    for (int i=0; i<CONCURRENT_MAX; i++) {
                        if (clients[i] != 0) {
                            send(clients[i], input_msg, BUFFER_SIZE, 0);
                        }
                    }
                    
                //new connect
                } else if (current_event.ident == server) {
                    struct sockaddr_in client_addr;
                    
                    socklen_t addr_len;
                    
                    int client_fd = accept(server, (struct sockaddr *)&client_addr, &addr_len);
                    
                    if (client_fd > 0) {
                        int index = -1;
                        
                        //put in clients array
                        for (int i = 0; i<CONCURRENT_MAX; i++) {
                            if (clients[i] == 0) {
                                index = i;
                                clients[i] = client_fd;
                                break;
                            }
                        }
                        
                        if (index >= 0) {
                            //监听该客户端输入
                            EV_SET(&changes, client_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                            //放入事件队列
                            kevent(kq, &changes, 1, NULL, 0, NULL);
                            
                            printf("新客户端（fd=%d）加入成功 %s:%d \n", client_fd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        } else {
                            bzero(input_msg, BUFFER_SIZE);
                            strcpy(input_msg, "服务器加入的客户端数已达最大值，无法加入\n");
                            send(client_fd, input_msg, BUFFER_SIZE, 0);
                            printf("新客户端加入失败 %s:%d \n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        }
                    }
                    
                //client new msg
                } else {

                    bzero(recv_msg, BUFFER_SIZE);
                    long byte_num = recv((int)current_event.ident,recv_msg,BUFFER_SIZE,0);
                    if (byte_num > 0) {
                        if (byte_num > BUFFER_SIZE) {
                            byte_num = BUFFER_SIZE;
                        }
                        recv_msg[byte_num] = '\0';
                        printf("客户端(fd = %d):%s\n",(int)current_event.ident,recv_msg);
                    }else if(byte_num < 0){
                        printf("从客户端(fd = %d)接受消息出错.\n",(int)current_event.ident);
                    }else{
                        EV_SET(&changes, current_event.ident, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                        kevent(kq, &changes, 1, NULL, 0, NULL);
                        close((int)current_event.ident);
                        for (int i = 0; i < CONCURRENT_MAX; i++) {
                            if (clients[i] == (int)current_event.ident) {
                                clients[i] = 0;
                                break;
                            }
                        }
                        printf("客户端(fd = %d)退出了\n",(int)current_event.ident);
                    }
                }
            }
        }
    }
}
