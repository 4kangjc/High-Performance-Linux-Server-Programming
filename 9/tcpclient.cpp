#include <sys/socket.h>
#include <sys/types.h>
#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>

#define MESSAGE_LEN 1024

int main(int argc, char* argv[]) {
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cout << "Failed to create!\n";
        exit(-1);
    }
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = PF_INET;
    serveraddr.sin_port = htons(12345);

    //serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    const char* ip = "127.0.0.1";
    inet_pton(AF_INET, ip, &serveraddr.sin_addr);


    int ret = connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if (ret < 0) {
        std::cout << "Connect Failed\n";
        exit(-1);
    } else {
        //printf("连接成功\n");
    }
    char sendbuff[MESSAGE_LEN] = {0};
    char recvbuff[MESSAGE_LEN] = {0};
    while (1) {
        memset(sendbuff, 0, MESSAGE_LEN);
        //std::getline(std::cin, sendbuff);
        fgets(sendbuff, MESSAGE_LEN, stdin);
        if (sendbuff[strlen(sendbuff) - 1] == '\n') {
            sendbuff[strlen(sendbuff) - 1] = '\0';
        }
        //std::cout << "send :" << sendbuff << std::endl;
        ret = send(sock, sendbuff, strlen(sendbuff), 0);
        if (ret <= 0) {
            std::cout << "Failed to send data!\n";
            break;
        }
        if (strcmp(sendbuff, "quit") == 0) {
            break;
        }
    }
    close(sock);
}
