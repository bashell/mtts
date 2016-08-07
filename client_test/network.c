#include "network.h"

ssize_t readn(int fd, void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while(nleft > 0) {
        if((nread = read(fd, bufp, nleft)) == -1) {
            if(errno == EINTR)  // interupt
                nread = 0;  // continue
            else
                return -1;  // ERROR
        } else if(nread == 0)  // EOF
            break;
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft);
}


ssize_t writen(int fd, void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nwrite;
    char *bufp = usrbuf;

    while(nleft > 0) {
        // nwrite == 0也属于错误
        if((nwrite = write(fd, bufp, nleft)) <= 0) {
            if(errno == EINTR) 
                nwrite = 0;
            else
                return ;
        }
        nleft -= nwrite;
        bufp += nwrite;
    }
    return n;
}

ssize_t recv_peek(int sockfd, void *buf, size_t len) {
    int nread;
    while(1) {
        nread = recv(sockfd, buf, len, MSG_PEEK);
        if(nread < 0 && errno == EINTR)
            continue;
        if(nread < 0)
            return -1;
        else
            break;
    }
    return nread;
}


ssize_t readline(int sockfd, void *buf, size_t maxline) {
    int nread;
    int nleft;
    char *ptr;
    int ret;
    int total = 0;

    nleft = maxline - 1;
    ptr = buf;

    while(nleft > 0) {
        ret = recv_peek(sockfd, ptr, nleft);
        if(ret <= 0)
            return ret;
        nread = ret;
        int i;
        for(i = 0; i < nread; ++i) {
            if(ptr[i] == '\n') {
                ret = readn(sockfd, ptr, i+1);
                if(ret != i+1)
                    return -1;
                total += ret;
                ptr += ret;
                *ptr = 0;
                return total;
            }
        }
        ret = readn(sockfd, ptr, nread);
        if(ret != nread) 
            return -1;
        nleft -= nread;
        total += nread;
        ptr += nread;
    }
    *ptr = 0;
    return maxline - 1;
}
