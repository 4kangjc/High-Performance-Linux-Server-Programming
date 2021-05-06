/**
 * @file multi_port.cpp
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-06
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>

const int MAX_EVENT_NUMBER = 1024;
const int TCP_BUFFER_SIZE = 512;
const int UDP_BUFFER_SIZE = 1024;

int setnonblocking(int fd) {
    int old_optition = fcntl(fd, F_GETFL);
    int new_optition = old_optition | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_optition);
    return old_optition;
}

void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
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

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    sockaddr_in addr = address;
    int ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    int udpfd = socket(PF_INET, SOCK_DGRAM, 0);
    ret = bind(udpfd, (sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(1314);
    assert(epollfd != -1);

    addfd(epollfd, listenfd);
    addfd(epollfd, udpfd);

    while (1) {
        auto number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0) {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (sockaddr*)&client_address, &client_addrlength);
                addfd(epollfd, connfd);
            } else if (sockfd == udpfd) {
                char buf[UDP_BUFFER_SIZE];
                memset(buf, '\0', UDP_BUFFER_SIZE);
                sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);

                ret = recvfrom(udpfd, buf, UDP_BUFFER_SIZE - 1, 0, (sockaddr*)&client_address, &client_addrlength);
                if (ret > 0) {
                    sendto(udpfd, buf, UDP_BUFFER_SIZE - 1, 0, (sockaddr*)&client_address, client_addrlength);
                }
            } else if (events[i].events & EPOLLIN) {
                char buf[TCP_BUFFER_SIZE];
                while (1) {
                    memset(buf, '\0', TCP_BUFFER_SIZE);
                    ret = recv(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);
                    if (ret < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        close(sockfd);
                        break;
                    } else if (ret == 0) {
                        close(sockfd);
                    } else {
                        send(sockfd, buf, ret, 0);
                    }
                }
            } else {
                printf("something else happened!\n");
            }
        }
    }
    close(listenfd);
    return 0;
}
