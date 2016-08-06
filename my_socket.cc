#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "my_socket.h"

/**
 * set fd nonblock.
 *
 * @param fd: file descriptor
 * @return 1 if success, or -1 if failed
 */
int mysocket::socket_set_nonblock(int fd) {
  int old_option, new_option;
  old_option = fcntl(fd, F_GETFL, NULL);
  if(old_option == -1)
    return -1;
  new_option = old_option | O_NONBLOCK;
  if(fcntl(fd, F_SETFL, new_option) == -1)
    return -1;
  return 1;
}

/**
 * set fd reusable.
 *
 * @param fd: file descriptor
 * @return 1 if success, or -1 if failed
 */
int mysocket::socket_set_reusable(int fd) {
  int on = 1;
  if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
    perror("setsockopt");
    return -1;
  }
  return 1;
}

/**
 * setup a socket connection.
 *
 * @param ip: IP address
 * @param port: port number
 * @return socket fd, or -1 if failed
 */
int mysocket::socket_setup(const char *ip, int port) {
  int sockfd;
  struct sockaddr_in sockaddr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd == -1) {
    perror("socket");
    return -1;
  }
  
  // set reuseable
  if(socket_set_reusable(sockfd) == -1) {
    fprintf(stderr, "Can't set socket fd %d reusable\n", sockfd);
    close(sockfd);
    return -1;
  }

  // set nonblocking
  if(socket_set_nonblock(sockfd) == -1) {
    fprintf(stderr, "Can't set socket fd %d nonblocking\n", sockfd);
    close(sockfd);
    return -1;
  }

  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(port);
  inet_pton(AF_INET, ip, &sockaddr.sin_addr);

  if(bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) == -1) {
    perror("bind");
    close(sockfd);
    return -1;
  }

  if(listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    close(sockfd);
    return -1;
  }  

  return sockfd;
}

