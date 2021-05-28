#pragma once

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
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

class process {
public:
    process() : m_pid(-1) {}
public:
    pid_t m_pid;
    int m_pipefd[2];
};

template <typename T>
class processpoll {
private:
    processpoll(int listenfd, int process_number = 8);
public:
    static processpoll<T>* create(int listenfd, int process_number = 8) {
        if (!m_instance) {
            m_instance = new processpoll<T>(listenfd, process_number);
        }
        return m_listance;
    }
    ~processpoll() {
        delete[] m_sub_process;
    }
    void run();
private:
    void setup_sig_pipe();
    void run_parent();
    void run_child();
private:
    static const int MAX_PROCESS_NUMBER = 16;   // 进程池允许的最大进程数量
    static const int USER_PER_PROCESS = 65535;  // 每个子进程最多能处理的客户数量
    static const int MAX_EVENT_NUMBER = 10000;  // epoll 最多能处理的客户数量
    int m_process_number;                       // 进程池的进程总数
    int m_idx = -1;                             // 子进程在池中的序号，从0开始
    int m_epollfd;                              // 每个进程都有一个epoll内核事件表，用m_epollfd表示
    int m_listenfd;                             // 监听socket
    bool m_stop = false;                        // 子进程通过m_stop来决定是否停止运行 
    process* m_sub_process;                     // 保存所有子进程的描述信息
    static processpoll<T>* m_instance;          // 进程池静态实例
};

template <typename T>
processpoll<T>* processpoll<T>::m_instance = nullptr;        // 定义

static int sig_pipefd[2];

static int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_option | O_NONBLOCK);
    return old_option;
}

static void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

static void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

static void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

static void addsig(int sig, void(*handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL));
}

template <typename T>
processpoll<T>::processpoll(int listenfd, int process_number) : m_listenfd(listenfd), m_process_number(process_number) {
    assert(process_number > 0 && process_number <= MAX_PROCESS_NUMBER);
    m_sub_process = new process[process_number];
    assert(m_sub_process);
    for (int i = 0; i < process_number; ++i) {
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert(ret == 0);

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);
        if (m_sub_process[i].m_pid > 0) {
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        } else {
            //close(m_listenfd);
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    } 
}

template <typename T>
void processpoll<T>::setup_sig_pipe() {
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);

    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd, sig_pipefd[0]);
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, sig_handler);
}

template <typename T>
void processpoll<T>::run() {
    if (m_idx != -1) {
        run_child();
        return;
    }
    run_parent();
}

template <typename T>
void processpoll<T>::run_child() {
    setup_sig_pipe();
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    addfd(m_epollfd, pipefd);

    epoll_event events[MAX_EVENT_NUMBER];
    T* users = new T[USER_PER_PROCESS];
    assert(users);
    int number = 0;
    int ret = -1;

    while (!m_stop) {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == pipefd && events[i].events & EPOLLIN) {
                int client = 0;
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if ((ret < 0 && errno != EAGAIN) || ret == 0) {
                    continue;
                } else {
                    sockaddr_in client_address;
                    socklen_t client_addrlength = sizeof(client_address);
                    int connfd = accept(m_listenfd, (sockaddr*)&client_address, &client_addrlength);
                    if (connfd < 0) {
                        printf("errno is: %d\n", errno);
                        continue;
                    }
                    addfd(m_epollfd, connfd);
                    users[connfd].init(m_epollfd, client_address);
                } 
            } else if (sockfd == sig_pipefd[0] && events[i].events & EPOLLIN) {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0) {
                    continue;
                } else {
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {
                            case SIGCHLD: {
                                pid_t pid;
                                int stat;
                                while (pid == waitpid(-1, &stat, WNOHANG) > 0) {
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT: {
                                m_stop = true;
                                break;
                            }
                            default:
                                break;
                        }
                    } 
                }
            } else if (events[i].data & EPOLLIN) {
                users[sockfd].process();
            } else {
                continue;
            }
        }
    }
    delete[] users;
    users = nullptr;
    close(pipefd);
    close(m_epollfd);
}

template <typename T>
void processpoll<T>::run_parent() {
    setup_sig_pipe();
    addfd(m_epollfd, m_listenfd);
    epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;
    
    while (!m_stop) {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            printf("epoll failture\n");
            break;
        }
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == m_listenfd) {
                int i = sub_process_counter;
                do {
                    if (m_sub_process[i].m_pid != -1) {
                        break;
                    }
                    i = (i + 1) % m_process_number;
                } while (i != sub_process_counter);

                if (m_sub_process[i].m_pid == -1) {
                    m_stop = true;
                    break;
                }
                sub_process_counter = (i + 1) % m_process_number;
                send(m_sub_process[i].m_pipefd[0], (char*)new_conn, sizeof(new_conn), 0);
                printf("send request to child %d\n", i);
            } else if (sockfd == sig_pipefd[0] && events[i].events & EPOLLIN) {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0) {
                    continue;
                } else {
                    for (int i  = 0; i < ret; ++i) {
                        switch (signals[i]) {
                            case SIGCHLD: {
                                pid_t pid;
                                int stat;
                                while (pid = waitpid(-1, &stat, WNOHANG) > 0) {
                                    for (int i = 0; i < m_process_number; ++i) {
                                        if (m_sub_process[i].m_pid == pid) {
                                            printf("child %d join\n", i);
                                            close(m_sub_process[i].m_pipefd[0]);
                                            m_sub_process[i].m_pid = 1;
                                        }
                                        m_stop = true;
                                        for (int i = 0; i < m_process_number; ++i) {
                                            if (m_sub_process[i].m_pid != -1) {
                                                m_stop = false;
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                            case SIGTERM:
                            case SIGINT: {
                                pritnf("kill all the child now\n");
                                for (int i = 0; i < m_process_number; ++i) {
                                    int pid = m_sub_process[i].m_pid;
                                    if (pid != -1) {
                                        kill(pid, STGTERM);
                                    }
                                }
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            } else {
                continue;
            }
        }
    }
    close(m_epollfd);
}