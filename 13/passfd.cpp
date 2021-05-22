/**
 * @file passfd.cpp
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-22
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static const int CONTROL_LEN = CMSG_LEN(sizeof(int));
/* 发送文件描述符，fd参数是用来传递信息的UNIX域socket，fd_to_send参数是待发送的文件描述符 */
void send_fd(int fd, int fd_to_send) {            // 复习下86页
    iovec iov[1];
    msghdr msg;
    char buf[0];

    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    cmsghdr cm;
    cm.cmsg_len = CONTROL_LEN;
    cm.cmsg_level = SOL_SOCKET;
    cm.cmsg_type = SCM_RIGHTS;
    *(int*)CMSG_DATA(&cm) = fd_to_send;
    msg.msg_control = &cm;
    msg.msg_controllen = CONTROL_LEN;

    sendmsg(fd, &msg, 0);
}

int recv_fd(int fd) {
    iovec iov[1];
    msghdr msg;
    char buf[0];

    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    cmsghdr cm;
    msg.msg_control = &cm;
    msg.msg_controllen = CONTROL_LEN;

    recvmsg(fd, &msg, 0);

    int fd_to_read = *(int*)CMSG_DATA(&cm);
    return fd_to_read;
}

int main() {
    int pipefd[2];
    int fd_to_pass = 0;

    int ret = socketpair(PF_UNIX, SOCK_DGRAM, 0, pipefd);
    assert(ret != -1);

    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0) {
        close(pipefd[0]);
        fd_to_pass = open("test.txt", O_RDWR, 0666);
        send_fd(pipefd[1], (fd_to_pass > 0) ? fd_to_pass : 0);
        close(fd_to_pass);
        exit(0);
    }

    close(pipefd[1]);
    fd_to_pass = recv_fd(pipefd[0]);
    char buf[1024];
    memset(buf, 0, 1024);
    read(fd_to_pass, buf, 1024);
    printf("I got fd %d and data is %s\n", fd_to_pass, buf);
    close(fd_to_pass);

    return 0;
}