#ifndef _MY_FD_H_
#define _MY_FD_H_

#include "my_mutex.h"

#define MAX_CONN 1024
#define TIMEOUT  60*5

typedef struct {
  int fd_;
  time_t visited_time_;
} sock_t;


class MyFd {
 public:
  MyFd();
  ~MyFd();

  void fd_set_init();
  void fd_set_destroy();
  int  fd_register(int fd);
  int  fd_remove(int fd);
  int  fd_update_visited_time(int fd, time_t visited_time);
  void fd_close_if_necessary(time_t now_time);

 private:
  MyFd(const MyFd&) = delete;
  MyFd &operator=(const MyFd&) = delete;
  
 private:
  sock_t *fd_set_;
  MutexLock mutex_;
};


#endif  /* _MY_FD_H_ */
