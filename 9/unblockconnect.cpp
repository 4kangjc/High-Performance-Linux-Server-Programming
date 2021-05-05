/**
 * @file unblockconnect.cpp
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-05
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

const int BUFFER_SIZE = 1023;

int setnonblocking(int fd) {
    int old_optition = fcntl(fd, F_GETFL);
    int new_optition = old_optition | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_optition);
    return old_optition;
}

int unblock_connect(const char* ip, int port, int time) {
    int ret = 0;
    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int fdopt = setnonblocking(sockfd);
    ret = connect(sockfd, (sockaddr*)&address, sizeof(address));
    if (ret == 0) {
        /* 连接成功，恢复sockfd的属性，并立即返回 */
        printf("connect with server immediately\n");
        fcntl(sockfd, F_SETFL, fdopt);
        return sockfd;
    } else if (errno != EINPROGRESS) {
        /* 如果连接没有立即建立，那么只有当errno是EINPROGRESS时才表示连接还在进行，否则出错返回 */
        printf("unblock connect not support!\n");
        return -1;
    }
    fd_set writefds;
    timeval timeout;
    FD_ZERO(&writefds);              // 书中这里应该不正确
    FD_SET(sockfd, &writefds);

    timeout.tv_sec = time;
    timeout.tv_usec = 0;

    ret = select(sockfd + 1, NULL, &writefds, NULL, &timeout);
    if (ret <= 0) {
        printf("connection time out!\n");
        return -1;
    } 
    if (!FD_ISSET(sockfd, &writefds)) {
        printf("no events on sockfd fount!\n");
        return -1;
    }
    int error = 0;
    socklen_t length = sizeof(error);
    /* 调用getsockopt来获取并清除sockfd上的错误 */
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &length)) {
        printf("get socket option failed\n");
        close(sockfd);
        return -1;
    }
    /* 错误号不为0表示连接出错 */
    if (error != 0) {
        printf("connection failed after select with the error: %d\n", error);
        close(sockfd);
        return -1;
    }
    printf("connection ready after select with the socket: %d\n", sockfd);
    fcntl(sockfd, F_SETFL, fdopt);
    return sockfd;
}

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = unblock_connect(ip, port, 10);
    if (sockfd < 0) {
        return 1;
    }
    close(sockfd);
    return 0;
}

// 1. 我们可以在三路握手的同时做一些其它的处理。connect 操作要花一个往返时间完成，而且可以是在任何地方，
//    从几个毫秒的局域网到几百毫秒或几秒的广域网，在这段时间内我们可能有一些其他的处理想要执行；
// 2. 可以用这种技术同时建立多个连接.在Web浏览器中很普遍；
// 3. 由于我们使用select 来等待连接的完，因此我们可以给select设置一个时间限制，从而缩短connect 的超时时间。在大多数实现中，
//    connect 的超时时间在75秒到几分钟之间。有时候应用程序想要一个更短的超时时间，使用非阻塞connect 就是一种方法。
