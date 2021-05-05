/**
 * @file recv_splice.cpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-01
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>


#define BUFFER_SIZE 512

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in serve_address;
    bzero(&serve_address, sizeof(serve_address));
    serve_address.sin_family = AF_INET;
    serve_address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serve_address.sin_addr);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    if (connect(sock, (struct sockaddr*)&serve_address, sizeof(serve_address)) != -1) {
        const char* s = "Hello World!";
        send(sock, s, strlen(s), 0);          //用于测试splice
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE);
        recv(sock, buffer, BUFFER_SIZE - 1, 0);
        printf("%s\n", buffer);
    } else {
        std::cout << "fail!\n";
    }

    close(sock);

    return 0;
}
