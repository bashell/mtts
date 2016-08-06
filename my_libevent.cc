#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "my_libevent.h"


MyLibevent::MyLibevent() : 
    last_thread_(-1), threads_total_num_(DEFAULT_MAXTHREADS), 
    threads_init_num_(0), cqi_flist_(NULL), threads_(NULL) 
{
  preparation();
}


MyLibevent::MyLibevent(int nthreads) : 
    last_thread_(-1), threads_total_num_(nthreads), 
    threads_init_num_(0), cqi_flist_(NULL), threads_(NULL)
{
  preparation();
} 


MyLibevent::~MyLibevent() {
  /* 添加各种free */
}


/**
 * Operations used in constructor
 */
void MyLibevent::preparation() {
  int ret;
  
  // initialize mutex
  ret = pthread_mutex_init(&init_lock_, NULL);
  if(ret != 0) {
    perror("pthread_mutex_init");
    exit(EXIT_FAILURE);
  }

  // initialize cond
  ret = pthread_cond_init(&init_cond_, NULL);
  if(ret != 0) {
    perror("pthread_cond_init");
    exit(EXIT_FAILURE);
  }
  
  // allocate memory for freelist of conn_queue
  cqi_flist_ = (CQ_ITEM_FLIST*)malloc(sizeof(CQ_ITEM_FLIST));
  if(NULL == cqi_flist_) {
    perror("Failed to allocate memory for freelist of connection queue.");
    exit(EXIT_FAILURE);
  }
  MyQueue::cqi_flist_init(cqi_flist_);

  // initialize fd_set
  myfd_.fd_set_init();
}


/**
 * Initialize libevent thread
 */
void MyLibevent::libevent_thread_init() {
  int i, ret;
  threads_ = (LIBEVENT_THREAD*)calloc(threads_total_num_, sizeof(LIBEVENT_THREAD));
  if(NULL == threads_) {
    perror("Can't allocate thread descriptors");
    exit(EXIT_FAILURE);
  }

  for(i = 0; i < threads_total_num_; ++i) {
    // 主线程与子线程之间建立管道
    int fds[2];
    if(pipe(fds)) {
      perror("Can't create notify pipe");
      exit(EXIT_FAILURE);
    }
    threads_[i].recv_fd_ = fds[0];  // 读端
    threads_[i].send_fd_ = fds[1];  // 写端

    threads_[i].ml_ptr_ = this;
    libevent_thread_setup(&threads_[i]);
  }

  for(i = 0; i < threads_total_num_; ++i) {
    create_worker(&threads_[i]);
  }

  // 等待所有线程设置结束后再返回
  pthread_mutex_lock(&init_lock_);
  while(threads_init_num_ < threads_total_num_)
    pthread_cond_wait(&init_cond_, &init_lock_);
  pthread_mutex_unlock(&init_lock_);
}


/**
 * 设置子线程信息
 */
void MyLibevent::libevent_thread_setup(LIBEVENT_THREAD *me) {
  me->base_ = event_base_new();
  if(NULL == me->base_) {
    fprintf(stderr, "Failed to allocate event base.\n");
    exit(EXIT_FAILURE);
  }

  // 子线程监听管道
  me->notify_event_ = event_new(me->base_, me->recv_fd_, EV_READ|EV_PERSIST, libevent_thread_process, (void*)me);
  if(event_add(me->notify_event_, NULL) == -1) {
    perror("Can't monitor pipe.");
    exit(EXIT_FAILURE);
  } 
  
  // 构造并初始化子线程的连接队列
  me->conn_queue_ = (CQ*)malloc(sizeof(CQ));
  if(NULL == me->conn_queue_) {
    perror("Failed to allocate memory for connection queue.");
    exit(EXIT_FAILURE);
  }
  MyQueue::cq_init(me->conn_queue_);
}


/**
 * 子线程管道可读时的回调
 */
void MyLibevent::libevent_thread_process(evutil_socket_t fd, short what, void *arg) {
  LIBEVENT_THREAD *me = (LIBEVENT_THREAD*)arg;
  CQ_ITEM *item = NULL;
  char buf[1];
  //struct timeval tv = {5, 0};

  if(read(fd, buf, 1) != 1)
    fprintf(stderr, "Can't read from libevent pipe.\n");

  printf("libevent_thread_process\n");

  item = MyQueue::cq_pop(me->conn_queue_)

  if(NULL != item) {
    struct bufferevent *bev = bufferevent_socket_new(me->base_, item->fd_, BEV_OPT_CLOSE_ON_FREE);
    if(NULL == bev) {
      fprintf(stderr, "Failed to construct bufferevent.\n");
      event_base_loopbreak(me->base_);
      exit(EXIT_FAILURE);
    }
      
    bufferevent_setcb(bev, readcb, writecb, eventcb, NULL);

    //bufferevent_set_timeouts(bev, &tv, &tv);  // set the read and write timeout for buffered event

    //bufferevent_enable(bev, EV_READ|EV_WRITE|EV_PERSIST);
    bufferevent_enable(bev, EV_READ | EV_WRITE);


    MyQueue::cqi_free(item, me->ml_ptr_->cqi_flist_);
  }
}


/**
 * 创建工作线程
 *
 * @param me: LIBEVENT_THREAD指针
 */
void MyLibevent::create_worker(void *arg) {
  int ret;
  pthread_attr_t attr;
  LIBEVENT_THREAD *me = (LIBEVENT_THREAD*)arg;

  pthread_attr_init(&attr);
  ret = pthread_create(&me->tid_, &attr, worker_libevent, arg);
  if(ret != 0) {
    fprintf(stderr, "Can't create thread: %s\n", strerror(ret));
    exit(EXIT_FAILURE);
  }

  printf("create_worker\n");
}


/**
 * 
 */
void* MyLibevent::worker_libevent(void *arg) {
  LIBEVENT_THREAD *me = (LIBEVENT_THREAD*)arg;

  printf("Worker thread %u started\n", (unsigned int)me->tid_);

  pthread_mutex_lock(&me->ml_ptr_->init_lock_);
  ++ me->ml_ptr_->threads_init_num_;
  pthread_cond_signal(&me->ml_ptr_->init_cond_);
  pthread_mutex_unlock(&me->ml_ptr_->init_lock_);
  
  event_base_dispatch(me->base_);

  return NULL;
}


/**
 *
 */
void MyLibevent::master_thread_loop(int sockfd, struct event_base *main_base) {
  struct timeval tv = {10, 0};
 
  // Setup dispatcher thread
  dispatcher_thread_.tid_ = pthread_self();
  dispatcher_thread_.base_ = main_base;
  dispatcher_thread_.ml_ptr_ = this;


  printf("Dispatcher thread %u started\n", (unsigned int)dispatcher_thread_.tid_);

  struct event *main_event = event_new(dispatcher_thread_.base_, sockfd, EV_READ|EV_PERSIST, accept_cb, (void*)&dispatcher_thread_);
  if(event_add(main_event, NULL) == -1) {
    perror("event_add failed");
    exit(EXIT_FAILURE);
  }
  
  //struct event *timeout_event = event_new(main_base_, -1, EV_TIMEOUT|EV_PERSIST, timeout_cb, NULL);
  struct event *timeout_event = evtimer_new(dispatcher_thread_.base_, timeout_cb, (void*)&dispatcher_thread_);
  if(event_add(timeout_event, &tv) == -1) {
    perror("event_add failed");
    exit(EXIT_FAILURE);
  }

  event_base_dispatch(dispatcher_thread_.base_);
}


/**
 * 连接请求处理
 */
void MyLibevent::accept_cb(int fd, short what, void *arg) {
  LIBEVENT_DISPATCHER_THREAD *me = (LIBEVENT_DISPATCHER_THREAD*)arg;
  int connfd;
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  memset(&client_addr, 0, sizeof(client_addr));

  connfd = accept(fd, (struct sockaddr*)&client_addr, &client_len);
  if(connfd == -1) {
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
      // ...
    } else {
      fprintf(stderr, "Too many open connections.\n");
      exit(EXIT_FAILURE);
    }
  }
  if(mysocket::socket_set_nonblock(connfd) == -1) {
    fprintf(stderr, "Failed to set nonblock.\n");
    close(connfd);
    exit(EXIT_FAILURE);
  }
  if(mysocket::socket_set_reusable(connfd) == -1) {
    fprintf(stderr, "Failed to set reusable.\n");
    close(connfd);
    exit(EXIT_FAILURE);
  }

  dispatch_conn_new(connfd, EV_READ|EV_PERSIST, arg);
  
  //event_base_loopexit(me->base_, NULL);
}


/**
 * 分配新连接
 */
void MyLibevent::dispatch_conn_new(int sfd, int ev_flag, void *arg) {
  LIBEVENT_DISPATCHER_THREAD *me = (LIBEVENT_DISPATCHER_THREAD*)arg;
  char buf[1];
  CQ_ITEM *item = MyQueue::cqi_new(me->ml_ptr_->cqi_flist_);
  if(NULL == item) {
    close(sfd);
    fprintf(stderr, "Failed to allocate memory for connection object\n");
    return ;
  }

  int tid = (me->ml_ptr_->last_thread_ + 1) % me->ml_ptr_->threads_total_num_;
  me->ml_ptr_->last_thread_ = tid;
  
  LIBEVENT_THREAD *thread = me->ml_ptr_->threads_ + tid;

  item->fd_ = sfd;
  item->ev_flag_ = ev_flag;
  
  MyQueue::cq_push(thread->conn_queue_, item);

  // dispatch

  buf[0] = 'c';
  if(write(thread->send_fd_, buf, 1) != 1)
    perror("Writing to thread notify pipe");
}


/**
 * 
 */
void MyLibevent::timeout_cb(int fd, short what, void *arg) {
  LIBEVENT_DISPATCHER_THREAD *me = (LIBEVENT_DISPATCHER_THREAD*)arg;
  printf("Timeout!\n");
  if(what & EV_TIMEOUT)
    me->ml_ptr_->myfd_.fd_close_if_necessary(time(NULL));
}


/**
 *
 */
void MyLibevent::readcb(struct bufferevent *bev, void *ctx) {
  char buf[1024];
  int n;

  struct evbuffer *input = bufferevent_get_input(bev);
  struct evbuffer *output = bufferevent_get_output(bev);
  int fd = bufferevent_getfd(bev);

  //time_t now = time(NULL);
  //fd_update_visited_time(fd, now);
  
  //evbuffer_add_buffer(output, input);
  while((n = evbuffer_remove(input, buf, sizeof(buf))) > 0) {
    printf("fd = %d, received: %s at thread = %ld\n", fd, buf, pthread_self());
    evbuffer_add(output, buf, n);
  }
}


/**
 *
 */
void MyLibevent::writecb(struct bufferevent *bev, void *ctx) {
  //...
}


/**
 *
 */
void MyLibevent::eventcb(struct bufferevent *bev, short events, void *ctx) {
  if(events & EV_TIMEOUT)
    fprintf(stderr, "Timeout.\n");
  if(events & BEV_EVENT_ERROR)
    fprintf(stderr, "Error from bufferevent.\n");
  if(events & (BEV_EVENT_ERROR | BEV_EVENT_EOF))
    bufferevent_free(bev);
  if(events & (BEV_EVENT_TIMEOUT | BEV_EVENT_READING))
    fprintf(stderr, "Read timeout.\n");
  if(events & (BEV_EVENT_TIMEOUT | BEV_EVENT_WRITING))
    fprintf(stderr, "Write timeout.\n");
}



int main()
{
  MyLibevent ml;
  
  int sockfd = mysocket::socket_setup("127.0.0.1", 8888);
  
  struct event_base *main_base = event_base_new();
  if(NULL == main_base) {
    fprintf(stderr, "Failed to construct event base");
    exit(EXIT_FAILURE);
  }

  ml.libevent_thread_init();
  ml.master_thread_loop(sockfd, main_base);
  return 0;
}
