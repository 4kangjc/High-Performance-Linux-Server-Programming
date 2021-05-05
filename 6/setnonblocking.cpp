/**
 * @file setnonblocking.cpp
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-02
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <fcntl.h>

int setnonblocking(int fd) {
    int old_optition = fcntl(fd, F_GETFL);
    int new_optition = old_optition | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_optition);
    return old_optition;
}
