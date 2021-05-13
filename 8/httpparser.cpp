/**
 * @file httpparser.cpp
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-13
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

static const int BUFFER_SIZE = 4096;

enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0,                        // 当前正在分析请求行
    CHECK_STATE_HEADER                                 // 当前正在分析头部字段
};

enum LINE_STATUS {
    LINE_OK = 0,                                       // 读取一个完整的行
    LINE_BAD,                                           // 行出错
    LINE_OPEN                                          // 行数据尚不完整
};

enum HTTP_CODE {
    NO_REQUEST,                                      // 请求不完整，需要继续读取客户数据
    GET_REQUEST,                                     // 获得了一个完整的客户请求
    BAD_REQUEST,                                      // 客户请求有语法错误
    FORBIDDEN_REQUEST,                               // 客户对资源没有足够的访问权限
    INTERNAL_ERROR,                                  // 服务器内部错误
    CLOSRD_CONNECTION                                // 客户端已经关闭连接了
};

static const char* szret[] = {"I get a correct result\n,", "something wrong\n"};

LINE_STATUS parse_line(char* buffer, int& checked_index, int& read_index) {
    /* checked_index指向buffer中当前正在分析的字节，read_index指向buffer中客户数据的尾部的下一字节，buffer中第0~checked_index字节都
    已分析完毕，第checked_index~(read_index-1)字节由下面的循环挨个分析*/
    for (; checked_index < read_index; ++checked_index) {
        char temp = buffer[checked_index];
        if (temp == '\r') {
            if ((checked_index + 1) == read_index) {
                return LINE_OPEN;
            } else if (buffer[checked_index + 1] == '\n') {
                buffer[checked_index++] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') {
            if (checked_index > 1 && buffer[checked_index - 1] == '\r') {
                buffer[checked_index - 1] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        return LINE_OPEN;
    }
}
// 分析请求行
HTTP_CODE parse_requestline(char* temp, CHECK_STATE& checkstate) {         // GET http://www.baidu.com/index.html HTTP/1.0
    char* url = strpbrk(temp, " \t");                                      // 字符 匹配函数
    // 如果请求行中没有空白字符或“\t"字符,则HTTP请求必有问题
    if (!url) {
        return BAD_REQUEST;
    }
    *url++ = '\0';

    char* method = temp;
    if (strcasecmp(method, "GET") == 0) {                                 // 忽略大小写的字符串比较函数
        printf("The request method is GET\n");
    } else {
        return BAD_REQUEST;
    }
    url += strspn(url, " \t");                                           // 防止还有空格或tab????
    char* version = strpbrk(url, " \t");
    if (!version) {
        return BAD_REQUEST;
    }
    *version++ = '\0';
    version += strspn(version, " \t");
    if (strcasecmp(version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }
    if (strncasecmp(url, "http://", 7) == 0) {
        url += 7;
        url = strchr(url, '/');                     //在一个串中查找给定字符的第一个匹配之处
    }
    if (!url || url[0] != '/') {
        return BAD_REQUEST;
    }
    printf("The request URL is %s\n", url);
    checkstate = CHECK_STATE_HEADER;
    return NO_REQUEST;
}
// 分析头部字段
HTTP_CODE parse_headers(char* temp) {                        // Host: www.baidu.com
    if (temp[0] == '\0') {
        return GET_REQUEST;
    } else if (strncasecmp(temp, "Host:", 5) == 0) {
        temp += 5;
        temp += strspn(temp, " \t");
        printf("the request host is: %s\n", temp);
    } else {
        printf("I can not handle this hander\n");
    }
    return NO_REQUEST;
}
// 分析HTTP请求的入口函数
HTTP_CODE parse_content(char* buffer, int& check_index, CHECK_STATE& checkstate, int& read_index, int& start_line) {
    LINE_STATUS linestatus = LINE_OK;
    HTTP_CODE retcode = NO_REQUEST;
    while ((linestatus = parse_line(buffer, check_index, read_index)) == LINE_OK) {
        char* temp = buffer + start_line;
        start_line = check_index;
        switch (checkstate) {
            case CHECK_STATE_REQUESTLINE: {
                retcode = parse_requestline(temp, checkstate);
                if (retcode == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                retcode = parse_headers(temp);
                if (retcode == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if (retcode == GET_REQUEST) {
                    return GET_REQUEST;
                }
                break;
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    if (linestatus == LINE_OPEN) {
        return NO_REQUEST;
    } else {
        return BAD_REQUEST;
    }
}

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        return;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);
    sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int fd = accept(listenfd, (sockaddr*)&client_address, &client_addrlength);
    if (fd < 0) {
        printf("errno is %d\n", errno);
    } else {
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int data_read = 0;
        int read_index = 0;
        int checked_index = 0;
        int start_line = 0;

        CHECK_STATE checkstate = CHECK_STATE_REQUESTLINE;
        while (1) {
            data_read = recv(fd, buffer + read_index, BUFFER_SIZE - read_index, 0);
            if (data_read == -1) {
                printf("reading failed\n");
                break;
            } else if (data_read == 0) {
                printf("remote client has closed the connection\n");
                break;
            }
            read_index += data_read;
            HTTP_CODE result = parse_content(buffer, checked_index, checkstate, read_index, start_line);
            if (result == NO_REQUEST) {
                continue;
            } else if (result == GET_REQUEST) {
                send(fd, szret[0], strlen(szret[0]), 0);
                break;
            } else {
                send(fd, szret[1], strlen(szret[1]), 0);
                break;
            }
        }
        close(fd);
    }
    close(listenfd);
    return 0;
}
