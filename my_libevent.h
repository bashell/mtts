#ifndef _MY_LIBEVENT_H_
#define _MY_LIBEVENT_H_

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <pthread.h>
#include "my_socket.h"
#include "my_queue.h"
#include "my_fd.h"

#define DEFAULT_MAXTHREADS 3

typedef struct {
  pthread_t tid_;
  int recv_fd_;
  int send_fd_;
  struct event_base *base_;
  struct event *notify_event_;
  CQ *conn_queue_;
  MyLibevent *ml_ptr_;
} LIBEVENT_THREAD;

typedef struct {
  pthread_t tid_;
  struct event_base *base_;
  MyLibevent *ml_ptr_;
} LIBEVENT_DISPATCHER_THREAD;


class MyLibevent {
 public:
  MyLibevent();
  MyLibevent(int nthreads);
  ~MyLibevent();

  void libevent_thread_init();
  void master_thread_loop(int sockfd, struct event_base *main_base);


  static void readcb(struct bufferevent *bev, void *ctx);
  static void writecb(struct bufferevent *bev, void *ctx);
  static void eventcb(struct bufferevent *bev, short events, void *ctx);

 private:
  MyLibevent(const MyLibevent&) = delete;
  MyLibevent &operator=(const MyLibevent&) = delete;
  
  void preparation(); 
  static void libevent_thread_setup(LIBEVENT_THREAD *me);
  static void libevent_thread_process(evutil_socket_t fd, short what, void *arg);

  static void create_worker(void *arg);
  static void *worker_libevent(void *arg);

  static void dispatch_conn_new(int sfd, int ev_flag, void *arg);
  static void accept_cb(int fd, short what, void *arg);
  static void timeout_cb(int fd, short what, void *arg);

 private:
  int last_thread_;
  int threads_total_num_;
  int threads_init_num_;

  pthread_mutex_t init_lock_;
  pthread_cond_t init_cond_;

  CQ_ITEM_FLIST *cqi_flist_;
  LIBEVENT_THREAD *threads_;

  LIBEVENT_DISPATCHER_THREAD dispatcher_thread_;
  MyFd myfd_;
};

#endif  /* _MY_LIBEVENT_H_ */
