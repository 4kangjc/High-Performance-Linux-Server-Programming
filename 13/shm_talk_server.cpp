/**
 * @file shm_talk_server.cpp
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-20
 * 
 * @copyright Copyright (c) 2021
 * 
 */
// 一个子进程处理一个客户连接        g++ shm_talk_server.cc -lrt -o shm_talk_server
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
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <functional>

static const int USER_LIMIT = 5;
static const int BUFFER_SIZE = 1024;
static const int FD_LIMIT = 65535;
static const int MAX_EVENT_NUMBER = 1024;
static const int PROCESS_LIMIT = 65535;

struct client_data {
    sockaddr_in address;                 // 客户端的socket地址
    int connfd;                          // socket文件描述符
    pid_t pid;                          // 处理这个连接的子进程的PID
    int pipefd[2];                      // 和父进程通信的管道 
};

static const char* shm_name = "/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd;

char* share_mem = 0;               // 共享内存首地址

client_data* users = 0;            // 客户连接数组。进程用客户连接的编号来索引这个数组，即可取得相关的客户连接数据
int* sub_process = 0;              // 子进程和客户连接的映射关系表，用进程的PID来索引这个数组，即可取得该进程所处理的客户连接的编号
int user_count = 0;               // 当前客户数量
bool stop_child = 0;

int setnonblocking(int fd) {                                // 将文件描述符设置为非阻塞
    int old_option = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_option | O_NONBLOCK);
    return old_option;
}

void addfd(int epollfd, int fd) {                          // 注册事件
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void sig_handler(int sig) {                               
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig, void (*handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;                                   // 让中断的系统重新调用
    }
    sigfillset(&sa.sa_mask);                                        // 目前理解 : 信号处理函数期间屏蔽所有信号
    assert(sigaction(sig, &sa, nullptr) != -1);
}

void del_resource() {
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    shm_unlink(shm_name);
    delete[] users;
    delete[] sub_process;
}

void child_term_hander(int sig) {
    stop_child = true;
}
/* 子进程运行的函数。参数idx指出该子进程处理的客户连接的编号，users是保存所有客户连接数据的数组 */
int run_child(int idx, client_data* users, char* share_mem) {
    epoll_event events[MAX_EVENT_NUMBER];
    int child_epollfd = epoll_create(5);
    assert(child_epollfd != -1);

    int connfd = users[idx].connfd;
    addfd(child_epollfd, connfd);
    int pipefd = users[idx].pipefd[1];
    addfd(child_epollfd, pipefd);
    int ret;

    addsig(SIGTERM, child_term_hander, false);

    while (!stop_child) {
        int number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {           // EINTR系统中断
            printf("epoll failture\n");
            break;
        } 
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == connfd && events[i].events & EPOLLIN) {
                memset(share_mem + idx * BUFFER_SIZE, '\0', BUFFER_SIZE);
                ret = recv(connfd, share_mem + idx * BUFFER_SIZE, BUFFER_SIZE - 1, 0);
                if (ret < 0) {
                    if (errno != EAGAIN) {                 // 如果读操作出错
                        stop_child = true;
                    }
                } else if (ret == 0) {
                    stop_child = true;
                } else {
                    /*成功读取用户数据后就通知主进程通过管道来处理 */
                    send(pipefd, (char*)&idx, sizeof(idx), 0);
                }
            } else if (sockfd == pipefd && events[i].events & EPOLLIN) {  // 主进程通知本进程(通过管道)将第client个客户数据发送到本进程负责的客户端
                int client = 0;
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if (ret < 0) {
                    if (errno != EAGAIN) {
                        stop_child = true;
                    }
                } else if (ret == 0) {
                    stop_child = true;
                } else {
                    send(connfd, share_mem + client * BUFFER_SIZE, BUFFER_SIZE, 0);
                } 
            } else {
                continue;
            }
        }
    }
    close(connfd);
    close(pipefd);
    close(child_epollfd);

    return 0;
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
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    user_count = 0;
    users = new client_data[USER_LIMIT + 1];
    sub_process = new int[PROCESS_LIMIT];
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        sub_process[i] = -1;
    }

    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);
    addfd(epollfd, sig_pipefd[0]);

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);            // kill 
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, sig_handler);
    bool stop_server = false;
    bool terminate = false;

    shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    assert(shmfd != -1);
    ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE);        // 参数fd指定的文件大小改为参数length指定的大小
    assert(ret != -1);

    share_mem = (char*)mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    assert(share_mem != MAP_FAILED);
    close(shmfd);

    while (!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            printf("epoll failture\n");
            break;
        }
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (sockaddr*)&client_address, &client_addrlength);
                if (connfd < 0) {
                    printf("errno is %d\n", errno);
                    continue;
                }
                if (user_count >= USER_LIMIT) {
                    const char* info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                } 
                users[user_count].address = client_address;
                users[user_count].connfd = connfd;
                /* 在主进程和子进程间建立管道，以传递必要的数据 */
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                assert(ret != -1);
                pid_t pid = fork();
                if (pid < 0) {
                    close(connfd);
                    continue;
                } else if (pid == 0) {
                    close(epollfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    run_child(user_count, users, share_mem);
                    munmap((void*)share_mem, USER_LIMIT * BUFFER_SIZE);
                    exit(0);
                } else {
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    addfd(epollfd, users[user_count].pipefd[0]);
                    users[user_count].pid = pid;
                    sub_process[pid] = user_count;
                    user_count++;
                }
            } else if (sockfd == sig_pipefd[0] && events[i].events & EPOLLIN) {
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    for (int j = 0; j < ret; ++j) {
                        switch (signals[j]) {
                            case SIGCHLD: {
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                    int del_user = sub_process[pid];
                                    sub_process[pid] = -1;
                                    if (del_user < 0 || del_user > USER_LIMIT) {
                                        continue;
                                    }
                                    /* 清空第del_user个客户连接使用的相关数据 */
                                    epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].pipefd[0], 0);
                                    close(users[del_user].pipefd[0]);
                                    users[del_user] = users[--user_count];
                                    sub_process[users[del_user].pid] = del_user;
                                    printf("child %d exit, now we have %d users\n", del_user, user_count); 
                                }
                                if (terminate && user_count == 0) {
                                    stop_server = true;
                                }
                                break;
                            } 
                            case SIGTERM:
                            case SIGINT: {
                                printf("kill all the child now\n");
                                if (user_count == 0) {
                                    stop_server = true;
                                    break;
                                }
                                for (int j = 0; j < user_count; ++j) {
                                    int pid = users[j].pid;
                                    kill(pid, SIGTERM);
                                }
                                terminate = true;
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            } else if (events[i].events & EPOLLIN) {                   // 某个子进程向父进程写入了数据
                int child = 0;
                ret = recv(sockfd, (char*)&child, sizeof(child), 0);
                printf("read data from child: %d accross pipe\n", child);
                if (ret == -1) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    for (int j = 0; j < user_count; ++j) {
                        if (users[j].pipefd[0] != sockfd) {
                            printf("send data to child accross pipe\n");
                            send(users[j].pipefd[0], (char*)&child, sizeof(child), 0);
                        }
                    }
                }
            }
        }
    }
    del_resource();
    return 0; 
}