#define _GNU_SOURCE 1  // 为了使用POLLRDHUP事件 socket上接收到对方关闭连接的请求之后触发
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 64

// ip_address port_number
int main(int argc, char* argv[])
{
    // 检查输入
    if (argc != 3) {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);


    // 绑定ser_address
    struct sockaddr_in serv_address;
    memset(&serv_address, 0, sizeof(serv_address));
    serv_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serv_address.sin_addr);
    serv_address.sin_port = htons(port);

    // 连接socket
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    if (connect(sockfd, (struct sockaddr*)&serv_address, sizeof(serv_address)) < 0) {
        printf("connect failed\n");
        close(sockfd);
        return 1;
    }

    // 注册两个文件描述符 
        // 标准输入 用户输入
        // sockfd上的可读事件 其他用户消息
    pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;


    // poll
    char read_buf[BUFFER_SIZE];
        // 为了零拷贝 建立管道
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);


    while (1) {
        ret = poll(fds, 2, -1);
        if (ret < 0) {
            printf("poll failure\n");
            break;
        }

        if (fds[1].revents & POLLRDHUP) {
            printf("server close the connection\n");
            break;
        }
        else if (fds[1].revents & POLLIN) {
            memset(read_buf, '\0', BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE-1, 0);
            printf("%s\n", read_buf);
        }

        if (fds[0].revents & POLLIN) {
            ret = splice(fds[0].fd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            assert(ret != -1);
            ret = splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            assert(ret != -1);
        }
    }

    // 关闭socket
    close(sockfd);
    return 0;
}

