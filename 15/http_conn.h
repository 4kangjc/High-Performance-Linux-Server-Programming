#pragma once
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "../14/locker.h"


class http_conn {
public:
    static const int FILENAME_LEN = 200;                            // 文件名的最长长度
    static const int READ_BUFFER_SIZE = 2048;                       // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;                      // 写缓冲区的大小
    enum class METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH };    // HTTP请求方法  65页
    enum class CHECK_STATE { 
        CHECK_STATE_REQUESTLINE,                                    // 当前正在分析请求行
        CHECK_STATE_HEADER,                                         // 当前正在分析头部字段
        CHECK_STATE_CONTENT                                         // 当前正在分析消息体
    };   //   主状态机所处的状态
    enum class HTTP_CODE { 
        NO_REQUEST,                                                 // 请求不完整，需要继续读取客户数据
        GET_REQUEST,                                                // 获得了一个完整的客户请求
        BAD_REQUEST,                                                // 客户请求有语法错误
        NO_RESOURCE,                                                // 
        FORBIDDEN_REQUEST,                                          // 客户对资源没有足够的访问权限
        FILE_REQUEST,                                               // 
        INTERNAL_ERROR,                                             // 服务器内部错误
        CLOSED_CONNECTION                                           // 客户端已经关闭连接了
    };    // HTTP请求的结果                                        
    enum class LINE_STATUS { 
        LINE_OK,                                                    // 读取一个完整的行
        LINE_BAD,                                                   // 行出错
        LINE_OPEN                                                   // 行数据尚不完整
    };   // 从状态机所处的状态

public:
    http_conn() {}
    ~http_conn() {}
public:
    void init(int sockfd, const sockaddr_in& addr);              // 初始化新接收的连接
    void close_conn(bool real_close = true);                    //   关闭连接
    void process();                                             // 处理客户请求
    bool read();                                                //  非阻塞读操作
    bool write();                                               // 非阻塞写操作
private:
    void init();                                                // 初始化连接
    HTTP_CODE process_read();                                   // 解析HTTP请求
    bool process_write(HTTP_CODE ret);                          // 填充HTTP应答
    // 下面这组函数被process_read调用以分析HTTP请求
    HTTP_CODE parse_request_line(char* text);                
    HTTP_CODE parse_handers(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    char* get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();
    // 下面这一组函数被process_write调用以填充HTTP应答
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_handers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;                                       // 所有的socket上的事件都被注册到同一个epoll文件描述符设置为静态的
    static int m_user_count;                                    // 用户数量
private:
    int m_sockfd;                                               // 该HTTP连接的socket和对方的socket地址
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE] = {0};                    // 读缓冲区
    int m_read_idx = 0;                                         // 标识符缓冲区中已经读入的客户数据的最后一个字节的下一个位置
    int m_check_idx = 0;                                        // 当前正在分析的字符在该缓冲区的位置
    int m_start_line = 0;                                       // 当前正在解析的行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE] = {0};                  // 写缓冲区   
    int m_write_idx = 0;                                        // 写缓冲区中待发送的字节数

    CHECK_STATE m_check_state = CHECK_STATE::CHECK_STATE_REQUESTLINE;                  // 主状态机当前所处的状态
    METHOD m_method = METHOD::GET;                               // 请求方法

    char m_real_file[FILENAME_LEN] = {0};            // 客户请求的目标文件的完整路径，其内容等于doc_root + m_url, doc_root是网站根目录
    char* m_url = 0;                                            // 客户请求的目标文件的文件名
    char* m_version = 0;                                        // HTTP协议版本号,我们仅支持HTTP/1.1
    char* m_host = 0;                                           // 主机名
    int m_content_length = 0;                                   // HTTP请求的消息体的长度
    bool m_linger = false;                                      // HTTP请求是否要保持连接

    char* m_file_address;                                       // 客户请求的目标文件被mmap到内存中的起始位置

    struct stat m_file_stat;                // 目标文件的状态。通过它我们可以判断文件是否存在，是否为目录，是否可读，并获取文件大小等信息
    iovec m_iv[2];                          // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量
    int m_iv_count;                         
};