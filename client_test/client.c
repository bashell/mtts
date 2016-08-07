#include <sys/epoll.h>
#include <libgen.h>
#include "network.h"

void do_service(int peerfd) {
    char recvbuf[1024] = {0};
    char sendbuf[1024] = {0};

    // 创建epoll句柄, fd
    int epollfd = epoll_create(2);
    if(epollfd == -1)
        ERR_EXIT("epoll_create");

    // 注册两个fd
    struct epoll_event ev;
    ev.data.fd = STDIN_FILENO;
    ev.events = EPOLLIN;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1)
        ERR_EXIT("epoll_ctl");
    ev.data.fd = peerfd;
    ev.events = EPOLLIN;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, peerfd, &ev) == -1)
        ERR_EXIT("epoll_ctl");

    // 准备一个数组
    struct epoll_event events[2];
    int nready;

    while(1) {
        // int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
        nready = epoll_wait(epollfd, events, 2, -1);
        if(nready == -1) {
            if(errno == EINTR)
                continue;
            else
                ERR_EXIT("epoll_wait");
        } else if(nready == 0)
            continue;

        // 开始检查每个fd
        int i;
        for(i = 0; i < nready; ++i) {
            int fd = events[i].data.fd;
            if(fd == STDIN_FILENO) {
                if(fgets(sendbuf, 1024, stdin) == NULL) {
                    shutdown(peerfd, SHUT_WR);  // 关闭写端
                    // 移除stdin这个fd
                    struct epoll_event ev;
                    ev.data.fd = STDIN_FILENO;
                    if(epoll_ctl(epollfd, EPOLL_CTL_DEL, STDIN_FILENO, &ev) == -1)
                        ERR_EXIT("epoll_ctl");
                } else {
                    writen(peerfd, sendbuf, strlen(sendbuf));
                }
            } else if(fd == peerfd) {
                int ret = readline(peerfd, recvbuf, 1024);
                if(ret == -1) {
                    ERR_EXIT("readline");
                } else if(ret == 0) {
                    close(peerfd);
                    printf("server close\n");
                    exit(EXIT_SUCCESS);
                } else {
                    printf("recv data: %s", recvbuf);
                }
            }
        }
    }
}


int main(int argc, char *argv[])
{
    if(argc != 3) {
        fprintf(stderr, "Usage: %s IP PORT\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    // socket
    int peerfd = socket(AF_INET, SOCK_STREAM, 0);
    if(peerfd == -1)
        ERR_EXIT("socket");

    struct sockaddr_in peeraddr;
    memset(&peeraddr, 0, sizeof(peeraddr));
    peeraddr.sin_family = AF_INET;
    peeraddr.sin_port = htons(port);
    peeraddr.sin_addr.s_addr = inet_addr(ip);
    
    // connect
    if(connect(peerfd, (struct sockaddr*)&peeraddr, sizeof(peeraddr)) == -1)
        ERR_EXIT("connect");

    do_service(peerfd);

    close(peerfd);
    return 0;
}
