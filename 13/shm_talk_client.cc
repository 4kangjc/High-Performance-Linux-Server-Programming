/**
 * @file shm_talk_client.cc
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-22
 * 
 * @copyright Copyright (c) 2021
 * 
 */   // 稍微改改 mytalk_client.cpp即可

#define _GUN_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>

const int BUFFER_SIZE = 1024;

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_address.sin_addr);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    if (connect(sockfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        printf("connection failed\n");
        close(sockfd);
        return 1;
    }

    pollfd fds[2];
    /* 注册文件描述符0(标准输入) 和 sockfd的可读事件*/
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;   // tcp连接被对方关闭，或者对方关闭了写操作
    fds[1].revents = 0;

    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);

    while (1) {
        ret = poll(fds, 2, -1);
        if (ret < 0) {
            printf("poll failure\n");
            break;
        }
        if (fds[1].revents & POLLRDHUP) {
            printf("serve close the connection\n");
            break;
        } else if (fds[1].revents & POLLIN) {
            memset(read_buf, '\0', BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE, 0);
            printf("%s", read_buf);
        }
        if (fds[0].revents & POLLIN) {
            ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
            assert(ret != -1);
            ret = splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
            assert(ret != -1);
        }
    }
    close(sockfd);
    return 0;
}