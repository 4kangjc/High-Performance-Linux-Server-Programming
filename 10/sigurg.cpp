/**
 * @file sigurg.cpp
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <functional>

const int BUFFER_SIZE = 1024;
static int connfd;

void sig_urg(int sig) {
    int save_errno = errno;
    char buffer[BUFFER_SIZE] = {0};
    int ret = recv(connfd, buffer, BUFFER_SIZE - 1, MSG_OOB);
    printf("got %d bytes of oob data '%s'\n", ret, buffer);
    errno = save_errno;
}

void addsig(int sig, void(*sig_handler)(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;    
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
}

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    if (ret == -1) {
        printf("errno is : %d\n", errno);
        return 1;
    }
    ret = listen(listenfd, 5);
    assert(ret != -1);

    sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    connfd = accept(listenfd, (sockaddr*)&client_address, &client_addrlength);
    if (connfd < 0) {
        printf("errno is : %d\n", errno);
    } else {
        addsig(SIGURG, sig_urg);
        fcntl(connfd, F_SETOWN, getpid());                     // 设置SIGURG信号的宿主进程

        char buffer[BUFFER_SIZE];
        while (1) {
            memset(buffer, '\0', sizeof(buffer));
            ret = recv(connfd, buffer, BUFFER_SIZE - 1, 0);
            if (ret <= 0) {
                break;
            } 
            printf("got %d bytes of normal data '%s'\n", ret, buffer);

        }
        close(connfd);
    }
    close(listenfd);
    return 0;
}
