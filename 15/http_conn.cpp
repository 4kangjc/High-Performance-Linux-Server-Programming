#include "http_conn.h"
#include <sys/uio.h>

const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_from = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_from = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_from = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_from = "There was an unusual problem serving the requested file.\n";

const char* doc_root = "/var/www/html";

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_option | O_NONBLOCK);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn(bool real_close) {
    if (real_close && (m_sockfd != -1)) {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::init(int sockfd, const sockaddr_in& addr) {
    m_sockfd = sockfd;
    m_address = addr;
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    //init();
}

void http_conn::init() {
    m_check_state = CHECK_STATE::CHECK_STATE_REQUESTLINE;
    m_linger = false;

    m_method = METHOD::GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_check_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for (; m_check_idx < m_read_idx; ++m_check_idx) {
        temp = m_read_buf[m_check_idx];
        if (temp == '\r') {
            if (m_check_idx + 1 == m_read_idx) {
                return LINE_STATUS::LINE_OPEN;
            } else if (m_read_buf[m_check_idx + 1] == '\n') {
                m_read_buf[m_check_idx++] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_STATUS::LINE_OK;
            }
        } else if (temp == '\n') {
            if (m_check_idx > 1 && m_read_buf[m_check_idx - 1] == '\r') {
                m_read_buf[m_check_idx - 1] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_STATUS::LINE_OK;
            }
            return LINE_STATUS::LINE_BAD;
        }
    }
    return LINE_STATUS::LINE_OPEN;
}

// ????????????????????????????????????????????????????????????????????????
bool http_conn::read() {
    if (m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        } else if (bytes_read == 0) {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

// ??????HTTP???????????????????????????????????????URL?????????HTTP?????????
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {  // GET http://localhost/index.html HTTP/1.1
    m_url = strpbrk(text, " \t");
    if (!m_url) {
        return HTTP_CODE::BAD_REQUEST;
    }
    *m_url++ = '\0';

    char* method = text;
    if (strcasecmp(method, "GET") == 0) {
        m_method = METHOD::GET;
    } else {
        return HTTP_CODE::BAD_REQUEST;
    }
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version) {
        return HTTP_CODE::BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0) {
        return HTTP_CODE::BAD_REQUEST;
    }
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/') {
        return HTTP_CODE::BAD_REQUEST;
    }
    m_check_state = CHECK_STATE::CHECK_STATE_HEADER;
    return HTTP_CODE::NO_REQUEST;
} 

// ??????HTTP???????????????????????????
http_conn::HTTP_CODE http_conn::parse_handers(char* text) {
    //?????????????????????????????????????????????
    if (text[0] == '\0') {
        //??????HTTP???????????????????????????????????????m_content_length???????????????????????????????????????CHECK_STATE_CONTENT??????
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE::CHECK_STATE_CONTENT;
            return HTTP_CODE::NO_REQUEST;
        } 
        return HTTP_CODE::GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    } else if (strncasecmp(text, "Content-Length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
    } else {
        printf("oop! unkown header %s\n", text);
    }
    return HTTP_CODE::NO_REQUEST;
}

// ???????????????????????????HTTP????????????????????????????????????????????????
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
    if (m_read_idx >= (m_content_length + m_check_idx)) {
        text[m_content_length] = '\0';
        return HTTP_CODE::GET_REQUEST;
    }
    return HTTP_CODE::NO_REQUEST;
}

// ????????????
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_STATUS::LINE_OK;
    HTTP_CODE ret = HTTP_CODE::NO_REQUEST;
    char* text = 0;

    while (((m_check_state == CHECK_STATE::CHECK_STATE_CONTENT && line_status == LINE_STATUS::LINE_OK)) 
        || ((line_status = parse_line()) == LINE_STATUS::LINE_OK)) {
         text = get_line();
         m_start_line = m_check_idx;
         printf("got 1 http line: %s\n", text);

         switch (m_check_state) {
             case CHECK_STATE::CHECK_STATE_REQUESTLINE: {
                 ret = parse_request_line(text);
                 if (ret == HTTP_CODE::BAD_REQUEST) {
                     return HTTP_CODE::BAD_REQUEST;
                 }
                 break;
             }
             case CHECK_STATE::CHECK_STATE_HEADER: {
                 ret = parse_handers(text);
                 if (ret == HTTP_CODE::BAD_REQUEST) {
                     return HTTP_CODE::BAD_REQUEST;
                 } else if (ret == HTTP_CODE::GET_REQUEST) {
                     return do_request();
                 }
                 break;
             }
             case CHECK_STATE::CHECK_STATE_CONTENT: {
                 ret = parse_content(text);
                 if (ret == HTTP_CODE::GET_REQUEST) {
                     return do_request();
                 }
                 line_status = LINE_STATUS::LINE_OPEN;
                 break;
             }
             default:
                return HTTP_CODE::INTERNAL_ERROR;
         }
    }
    return HTTP_CODE::NO_REQUEST;
}

// ?????????????????????,?????????HTTP??????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
// ??????mmap???????????????????????????m_file_address??????????????????????????????????????????
http_conn::HTTP_CODE http_conn::do_request() {
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    if (stat(m_real_file, &m_file_stat) < 0) {
        return HTTP_CODE::NO_REQUEST;
    }
    if (!(m_file_stat.st_mode & S_IROTH)) {
        return HTTP_CODE::FORBIDDEN_REQUEST;
    }
    if (S_ISDIR(m_file_stat.st_mode)) {
        return HTTP_CODE::BAD_REQUEST;
    }
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return HTTP_CODE::FILE_REQUEST;
}

// ????????????????????????mumap??????
void http_conn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = NULL;
    }
}

// ???HTTP??????        
bool http_conn::write() {
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if (bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }
    while (1) {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp <= -1) {
            // ??????TCP??????????????????????????????????????????EPOLLOUT????????????????????????????????????????????????????????????????????????????????????????????????
            // ?????????????????????????????????
            if (errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if (bytes_to_send <= bytes_have_send) {
            // ??????HTTP?????????????????????HTTP????????????Connection??????????????????????????????
            unmap();
            if (m_linger) {
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            } else {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}

// ???????????????????????????????????????
bool http_conn::add_response(const char* format, ...) {
    if (m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool http_conn::add_status_line(int status, const char* title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", "status", title);
}

bool http_conn::add_handers(int content_len) {
    add_content_length(content_len);
    add_linger();
    add_blank_line();
    return true;
}

bool http_conn::add_content_length(int content_len) {
    return add_response("Content-Length: %d\r\n", content_len);
}

bool http_conn::add_linger() {
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line() {
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char* content) {
    return add_response("%s", content);
}

//?????????????????????HTTP???????????????????????????????????????????????????
bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret) {
        case HTTP_CODE::INTERNAL_ERROR: {
            add_status_line(500, error_500_title);
            add_handers(strlen(error_500_from));
            if (!add_content(error_500_from)) {
                return false;
            }
            break;
        }
        case HTTP_CODE::BAD_REQUEST: {
            add_status_line(400, error_400_title);
            add_handers(strlen(error_400_from));
            if (!add_content(error_400_from)) {
                return false;
            }
            break;
        }
        case HTTP_CODE::NO_RESOURCE: {
            add_status_line(404, error_404_title);
            add_handers(strlen(error_404_from));
            if (!add_content(error_404_from)) {
                return false;
            }
            break;
        }
        case HTTP_CODE::FORBIDDEN_REQUEST: {
            add_status_line(403, error_403_title);
            add_handers(strlen(error_403_from));
            if (!add_content(error_403_from)) {
                return false;
            }
            break;
        }
        case HTTP_CODE::FILE_REQUEST: {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0) {
                add_handers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            } else {
                const char* ok_string = "<html><body></body></html>";
                add_handers(strlen(ok_string));
                if (!add_content(ok_string)) {
                    return false;
                }
            }
        }
        default: {
            return false;
        }
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

void http_conn::process() {
    HTTP_CODE read_ret = process_read();
    if (read_ret == HTTP_CODE::NO_RESOURCE) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret) {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}