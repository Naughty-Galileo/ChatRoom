#define _GNU_SOURCE 1  // 为了使用POLLRDHUP事件 socket上接收到对方关闭连接的请求之后触发
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>

#define USER_LIMIT 5 /*最大用户数量*/
#define BUFFER_SIZE 64 /*读缓冲区大小*/
#define FD_LIMIT 65535 /*文件描述符数量限制*/

struct client_data
{
    sockaddr_in address;
    char* write_buf;
    char buf[BUFFER_SIZE];
};

int setnoblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int main(int argc, char* argv[])
{
    // 检查输入
    if (argc != 2) {
        printf("usage: %s port_number\n", basename(argv[0]));
        return 1;
    }
    // const char* ip = argv[1];
    int port = atoi(argv[1]);
    
    int ret;

    // 绑定地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    // inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >=0 );

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    client_data* user = new client_data[FD_LIMIT];

    // 为了提高poll性能 限制用户数量
    pollfd fds[USER_LIMIT+1];
    int user_count = 0;


    // 注册事件
    for (int i = 1; i <= USER_LIMIT; ++i) {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    while (1) {
        ret = poll(fds, user_count+1, -1);
        if (ret < 0) {
            printf("poll failure\n");
            break;
        }

        for (int i = 0; i < user_count+1; ++i) {
            
            // 连接请求
            if ( (fds[i].fd == listenfd) && (fds[i].revents & POLLIN) ) {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);

                if (connfd < 0) {
                    printf("errno is %d\n", errno);
                    continue;
                }

                if (user_count >= USER_LIMIT) {
                    const char* info = "too many users,wait...\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }

                user_count ++;
                user[connfd].address = client_address;
                setnoblocking(connfd);
                
                fds[user_count].fd = connfd;
                fds[user_count].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_count].revents = 0;
                printf("comes a new user %d, now have %d users\n", connfd, user_count);
            }
            // 错误
            else if (fds[i].revents & POLLERR) {
                printf("get an error from %d\n", fds[i].fd);
                char errors[100];
                memset(errors, '\0', 100);
                socklen_t length = sizeof(errors);
                if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0){
                    printf("get socket option failed\n");
                }
            }
            // 关闭连接
            else if (fds[i].revents & POLLRDHUP) {
                user[fds[i].fd] = user[fds[user_count].fd];
                close(fds[i].fd);
                fds[i] = fds[user_count];
                i--;
                user_count--;
                printf("a client left\n");
            }
            // 从用户读取数据
            else if (fds[i].revents & POLLIN) {
                int connfd = fds[i].fd;
                memset(user[connfd].buf, '\0', BUFFER_SIZE);
                ret = recv(connfd, user[connfd].buf, BUFFER_SIZE-1, 0);
                printf("get %d bytes of client data %s from %d\n", ret, user[connfd].buf, connfd);

                if (ret < 0) {
                    if (errno != EAGAIN) {
                        close(connfd);
                        user[fds[i].fd] = user[fds[user_count].fd];
                        fds[i] = fds[user_count];
                        i--;
                        user_count--;
                    }
                }
                else if (ret == 0) { printf("continue...\n"); }
                else {
                    // 通知其他用户socket
                    for (int j = 1; j <= user_count; ++j) {
                        if (fds[j].fd == connfd) { continue; }

                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        user[fds[j].fd].write_buf = user[connfd].buf;
                    }
                }
            }
            // 发送数据
            else if(fds[i].revents & POLLOUT) {
                int connfd = fds[i].fd;
                if (!user[connfd].write_buf) {
                    continue;
                }
                ret = send(connfd, user[connfd].write_buf, strlen(user[connfd].write_buf), 0);
                assert(ret != -1);
                
                user[connfd].write_buf = NULL;
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }

    delete[] user;
    close(listenfd);
    return 0;
}




