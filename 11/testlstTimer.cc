/**
 * @file testlstTimer.cc
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-08
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
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include "lstTimer.h"
#include "../6/setnonblocking.h"

const int FD_LIMIT = 65535;
const int MAX_EVENT_NUMBER = 1024;
const int TIMESLOT = 5;

static int pipefd[2];
static sortTimerList timerLst;
static int epollfd = 0;

void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);                         // 屏蔽所有信号？？？不懂
    assert(sigaction(sig, &sa, nullptr) != -1);
}

void time_handler() {
    timerLst.tick();
    /* 因为一次alarm调用只会引起一次SIGALRM信号，所以我们要重新定时，以不断触发SIGALRM信号 */
    alarm(TIMESLOT);
}

//  定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之
void cb_func(clientData* userData) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, userData->sockfd, 0);
    assert(userData);
    close(userData->sockfd);
    printf("close fd %d\n", userData->sockfd);
}

int main(int argc, char** argv) {
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

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    addsig(SIGALRM);
    addsig(SIGTERM);
    addsig(SIGINT);
    bool stop_server = false;

    clientData* users = new clientData[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);

    while (!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            printf("epoll failtrue!\n");
            break;
        }
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (sockaddr*)&client_address, &client_addrlength);
                addfd(epollfd, connfd);
                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;


                utilTimer* timer = new utilTimer();
                timer->userData = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                timer->expire = cur + 3 * TIMESLOT;
                users[connfd].timer = timer;
                timerLst.addTimer(timer);

                printf("comes a new user, its connfd is %d\n", connfd); 
                
            } else if (sockfd == pipefd[0] && events[i].events & EPOLLIN) {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0) {
                    continue;
                } else {
                    for (int j = 0; j < ret; ++j) {
                        switch(signals[j]) {
                            case SIGALRM: {
                                timeout = true;
                                break;
                            }
                            case SIGTERM: {
                                stop_server = true;
                            }
                            case SIGINT: {
                                printf("ctrl c!\n");
                                stop_server = true;
                            }
                        }
                    }
                }
            } else if (events[i].events & EPOLLIN) {
                memset(users[sockfd].buf, 0, BUFFER_SIZE);
                ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0);
                printf("get %d bytes of client data '%s' from %d\n", ret, users[sockfd].buf, sockfd);
                utilTimer* timer = users[sockfd].timer;
                if (ret < 0 || ret == 0) {
                    if (errno != EAGAIN || ret == 0) {
                        cb_func(&users[sockfd]);
                        if (timer) {
                            timerLst.delTimer(timer);
                        }
                    }
                } else {
                    if (timer) {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        printf("addjust timer once\n");
                        timerLst.adjustTimer(timer);
                    } else {
                        // ...
                    }
                }
            }
            if (timeout) {
                time_handler();
                timeout = false;
            }
        }
    }
    close(listenfd);
    for (int i = 0; i < 2; ++i) {
        close(pipefd[i]);
    } 
    delete[] users;

    return 0;
}
