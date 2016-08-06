#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "my_fd.h"
#include "my_socket.h"


MyFd::MyFd() : fd_set_(NULL) {}

MyFd::~MyFd() {}

/**
 * Initialize fd set.
 */
void MyFd::fd_set_init() {
  int i;

  fd_set_ = (sock_t*)malloc(MAX_CONN * sizeof(sock_t));
  if(NULL == fd_set_) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  for(i = 0; i < MAX_CONN; ++i) {
    fd_set_[i].fd_ = -1;
    fd_set_[i].visited_time_ = 0;
  }
}

/**
 * Destroy fd set.
 */
void MyFd::fd_set_destroy() {
  free(fd_set_);
}

/**
 * Register a fd on the fd_set.
 *
 * @param fd: fd to be added
 * @return 1 if success, or -1 when failed
 */
int MyFd::fd_register(int fd) {
  if(fd < 0 || fd > MAX_CONN-1)
    return -1;

  MutexLockGuard lock(mutex_);
  fd_set_[fd].fd_ = fd;
  fd_set_[fd].visited_time_ = time(NULL);
  printf("fd = %d is added, time = %ld\n", fd, fd_set_[fd].visited_time_);
  
  return 1;
}

/**
 * Remove a fd from the fd_set.
 *
 * @param fd: fd to be removed
 * @return 1 if success, or -1 when failed
 */
int MyFd::fd_remove(int fd) {
  if(fd < 0 || fd > MAX_CONN-1)
    return -1;

  MutexLockGuard lock(mutex_);
  close(fd);
  fd_set_[fd].fd_ = -1;
  fd_set_[fd].visited_time_ = 0;
  printf("fd = %d is removed, time = %ld\n", fd, fd_set_[fd].visited_time_);
  
  return 1;
}

/**
 * update the last visited time of fd
 *
 * @param fd: file descriptor
 * @param visited_time: last visited time
 * @return 1 is success, or -1 when failed
 */
int MyFd::fd_update_visited_time(int fd, time_t visited_time) {
  MutexLockGuard lock(mutex_);
  if(fd_set_[fd].fd_ != -1) {
    fd_set_[fd].visited_time_ = visited_time;
    return 1;
  }
  return -1;
}

/**
 * close the fd if the time of no operation is larger than TIMEOUT
 *
 * @param now_time: the time now
 */
void MyFd::fd_close_if_necessary(time_t now_time) {
  int i;
  for(i = 0; i < MAX_CONN; ++i) {
    if(fd_set_[i].fd_ != -1 && now_time - fd_set_[i].visited_time_ >= TIMEOUT) 
        fd_remove(fd_set_[i].fd_);
  }
}


/*
int main()
{
  MyFd mf;
  //ms.socket_setup("127.0.0.1", 8888);
  int fd = mysocket::socket_setup("127.0.0.1", 8888);
  printf("%d\n", fd);
  return 0;
}
*/
