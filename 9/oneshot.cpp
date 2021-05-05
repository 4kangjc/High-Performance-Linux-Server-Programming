/**
 * @file oneshort.cpp
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
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <thread>

const int MAX_EVENT_NUMBER = 1024;
const int BUFFER_SIZE = 10;

int setnonblocking(int fd) {
    int old_optition = fcntl(fd, F_GETFL);
    int new_optition = old_optition | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_optition);
    return old_optition;
}

void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void reset_oneshot(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void worker(int sockfd, int epollfd) {
    printf("start new thread to receive data on fd: %d\n", sockfd);
    char buf[BUFFER_SIZE];
    memset(buf, '\0', BUFFER_SIZE);
    while (1) {
        int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
        if (ret == 0) {
            close(sockfd);
            printf("foreiner closed the connection\n");
            break;
        } else if (ret < 0) {
            //printf("ret = %d\n", ret);
            if( errno == EAGAIN ) {
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        } else {
            printf("get content: %s\n", buf);
            memset(buf, '\0', BUFFER_SIZE);
            //sleep(5);
        }
    }
    printf("end thread receiving data on fd: %d\n", sockfd);
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

    ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(520);
    assert(epollfd != -1);
    addfd(epollfd, listenfd, false);

    while (1) {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0) {
            printf( "epoll failure\n" );
            break;
        }
        for (int i = 0; i < ret; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (sockaddr*)&client_address, &client_addrlength);
                addfd(epollfd, connfd, true);
            } else if (events[i].events & EPOLLIN) {
                std::thread t(worker, sockfd, epollfd);
                t.join();
            } else {
                printf("something else happend\n");
            }
        }
    }
    close(listenfd);
    return 0;
}
