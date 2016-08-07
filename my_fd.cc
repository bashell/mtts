#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "my_fd.h"
#include "my_socket.h"


MyFd::MyFd() : fd_set_(NULL) {}

MyFd::~MyFd() {
  fd_set_destroy();
}


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

#ifdef GET_FD_LOG
  char ch_array[CHAR_ARRAY_SIZE];
  time_t m_time = time(NULL);
  get_format_time(ch_array, sizeof(ch_array), localtime(&m_time));
  printf("fd_set is initialized, time = %s\n", ch_array);
#endif

}


/**
 * Destroy fd set.
 */
void MyFd::fd_set_destroy() {
  free(fd_set_);

#ifdef GET_FD_LOG
  char ch_array[CHAR_ARRAY_SIZE];
  time_t m_time = time(NULL);
  get_format_time(ch_array, sizeof(ch_array), localtime(&m_time));
  printf("\nfd_set is freed, time = %s\n", ch_array);
#endif

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
  if(fd_set_[fd].fd_ == -1)
    fd_set_[fd].fd_ = fd;
  fd_set_[fd].visited_time_ = time(NULL);
 
#ifdef GET_FD_LOG
  char ch_array[CHAR_ARRAY_SIZE];
  get_format_time(ch_array, sizeof(ch_array), localtime(&fd_set_[fd].visited_time_));
  printf("\nfd = %d is added, time = %s\n", fd, ch_array);
#endif

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

#ifdef GET_FD_LOG
  char ch_array[CHAR_ARRAY_SIZE];
  time_t m_time = time(NULL);
  get_format_time(ch_array, sizeof(ch_array), localtime(&m_time));
  printf("\nfd = %d is removed, time = %s\n", fd, ch_array);
#endif
  
  return 1;
}


/**
 * Update the last visited time of fd.
 *
 * @param fd: file descriptor
 * @param visited_time: last visited time
 * @return 1 is success, or -1 when failed
 */
int MyFd::fd_update_visited_time(int fd, time_t visited_time) {
  MutexLockGuard lock(mutex_);
  if(fd_set_[fd].fd_ != -1) {
    fd_set_[fd].visited_time_ = visited_time;

#ifdef GET_FD_LOG
    char ch_array[CHAR_ARRAY_SIZE];
    get_format_time(ch_array, sizeof(ch_array), localtime(&fd_set_[fd].visited_time_));
    printf("Last visited time of fd = %d is updated, time = %s\n", fd, ch_array);
#endif

    return 1;
  }
  return -1;
}


/**
 * Close the fd if the time of no operation is larger than 'NO_OPERATION_DURATION'.
 *
 * @param now_time: the time now
 */
void MyFd::fd_close_if_necessary(time_t now_time) {
  int i;
  for(i = 0; i < MAX_CONN; ++i) {
    if(fd_set_[i].fd_ != -1 && now_time - fd_set_[i].visited_time_ > NO_OPERATION_DURATION) 
      fd_remove(fd_set_[i].fd_);
  }
}


#ifdef GET_FD_LOG
/**
 * Get format time.
 *
 * @param s: character array used to store result
 * @param max: the size of array
 * @param tm: pointer to struct tm
 */
void MyFd::get_format_time(char *s, size_t max, const struct tm *tm) {
  strftime(s, max, "%Y-%m-%d|%H:%M:%S", tm);
}
#endif


