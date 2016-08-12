#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "my_libevent.h"


MyLibevent::MyLibevent() 
  : last_thread_(-1), threads_total_num_(MAX_SLAVE_THREAD_NUM), 
    threads_init_num_(0), cqi_flist_(NULL), 
    threads_(NULL), dispatcher_thread_(NULL)
{
  start_operations();
}


MyLibevent::MyLibevent(int nthreads) : 
    last_thread_(-1), threads_total_num_(nthreads), 
    threads_init_num_(0), cqi_flist_(NULL),
    threads_(NULL), dispatcher_thread_(NULL)
{
  start_operations();
} 


MyLibevent::~MyLibevent() {
  stop_operations();
}


/**
 * Operations used in constructor
 */
void MyLibevent::start_operations() {
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
  
  // allocate memory for freelist of connection queue
  cqi_flist_ = (CQ_ITEM_FLIST*)malloc(sizeof(CQ_ITEM_FLIST));
  if(NULL == cqi_flist_) {
    perror("Failed to allocate memory for freelist of connection queue.");
    exit(EXIT_FAILURE);
  }
  MyQueue::cqi_flist_init(cqi_flist_);

  // allocate memory for slave threads
  threads_ = (LIBEVENT_THREAD*)calloc(threads_total_num_, sizeof(LIBEVENT_THREAD));
  if(NULL == threads_) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }

  // allocate memory for master thread
  dispatcher_thread_ = (LIBEVENT_DISPATCHER_THREAD*)calloc(1, sizeof(LIBEVENT_DISPATCHER_THREAD));
  if(NULL == dispatcher_thread_) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }

  // initialize fd_set
  myfd_.fd_set_init();
}


/**
 * Operations used in destructor
 */
void MyLibevent::stop_operations() {
  int i;
  char buf[1];

  // notify slave threads to stop
  buf[0] = 'p';
  for(i = 0; i < threads_init_num_; ++i) {
    if(write(threads_[i].send_fd_, buf, 1) != 1)
      perror("Writing to thread notify pipe");
  }

  // master thread stop
  event_base_loopexit(dispatcher_thread_->base_, NULL);

  // destroy mutex and cond
  pthread_mutex_destroy(&init_lock_);
  pthread_cond_destroy(&init_cond_);
  
  // free freelist of connection
  MyQueue::cqi_flist_destroy(cqi_flist_);

  // free event_base of master and slave threads
  for(i = 0; i < threads_init_num_; ++i)
    event_base_free(threads_[i].base_);
  event_base_free(dispatcher_thread_->base_);

  // free connection queue of slave threads
  for(i = 0; i < threads_init_num_; ++i)
    MyQueue::cq_destroy(threads_[i].conn_queue_);
  
  free(threads_); 
  free(dispatcher_thread_);
}


/**
 * Sever start.
 *
 * @param sockfd: socket fd
 * @param main_base: pointer ot dispatcher event_base
 */
void MyLibevent::server_start(int sockfd, struct event_base *main_base) {
  libevent_thread_init();
  master_thread_loop(sockfd, main_base);
}


/**
 * Initialize libevent thread
 */
void MyLibevent::libevent_thread_init() {
  int i;

  for(i = 0; i < threads_total_num_; ++i) {
    // setup pipe between master and slave threads
    int fds[2];
    if(pipe(fds)) {
      perror("Can't create notify pipe");
      exit(EXIT_FAILURE);
    }
    threads_[i].recv_fd_ = fds[0];  // read
    threads_[i].send_fd_ = fds[1];  // write

    threads_[i].ml_ptr_ = this;
    libevent_thread_setup(&threads_[i]);
  }

  for(i = 0; i < threads_total_num_; ++i) {
    create_worker(&threads_[i]);
  }

  // wait until all slave threads have been setup
  pthread_mutex_lock(&init_lock_);
  while(threads_init_num_ < threads_total_num_)
    pthread_cond_wait(&init_cond_, &init_lock_);
  pthread_mutex_unlock(&init_lock_);
}


/**
 * setup LIBEVENT_THREAD (slave thread)
 *
 * @param me: pointer to slave thread
 */
void MyLibevent::libevent_thread_setup(LIBEVENT_THREAD *me) {
  me->base_ = event_base_new();
  if(NULL == me->base_) {
    fprintf(stderr, "Failed to allocate event base.\n");
    exit(EXIT_FAILURE);
  }

  // slave thread monitors read-end of pipe
  me->notify_event_ = event_new(me->base_, 
                                me->recv_fd_, 
                                EV_READ|EV_PERSIST, 
                                libevent_thread_process, 
                                (void*)me);
  if(event_add(me->notify_event_, NULL) == -1) {
    perror("Can't monitor pipe.");
    exit(EXIT_FAILURE);
  }
  
  // allocate memory for connection queue
  me->conn_queue_ = (CQ*)malloc(sizeof(CQ));
  if(NULL == me->conn_queue_) {
    perror("Failed to allocate memory for connection queue.");
    exit(EXIT_FAILURE);
  }
  MyQueue::cq_init(me->conn_queue_);
}


/**
 * Callback function called when pipe is readable
 */
void MyLibevent::libevent_thread_process(evutil_socket_t fd, short what, void *arg) {
  LIBEVENT_THREAD *me = (LIBEVENT_THREAD*)arg;
  CQ_ITEM *item = NULL;
  char buf[1];
  struct timeval tv_timeout = {60, 0};

  if(read(fd, buf, 1) != 1)
    fprintf(stderr, "Can't read from libevent pipe.\n");

#ifdef PRINT_DEBUG
  printf("Connected, thread %lu ready to process!\n", me->tid_);
#endif

  switch(buf[0]) {
    // connection
    case 'c':  
    item = MyQueue::cq_pop(me->conn_queue_);
  
    if(NULL != item) { 
      struct bufferevent *bev = bufferevent_socket_new(me->base_, 
                                                       item->fd_, 
                                                       BEV_OPT_CLOSE_ON_FREE);
      if(NULL == bev) {
        fprintf(stderr, "Failed to construct bufferevent.\n");
        event_base_loopbreak(me->base_);
        exit(EXIT_FAILURE);
      }
    
      // setup callback_fn
      bufferevent_setcb(bev, readcb, writecb, eventcb, arg);
 
      // set the read and write timeout for bufferevent
      bufferevent_set_timeouts(bev, &tv_timeout, &tv_timeout);  

      bufferevent_enable(bev, EV_READ|EV_WRITE);

      MyQueue::cqi_free(item, me->ml_ptr_->cqi_flist_);
    }
    break;

    // pause
    case 'p':
    event_base_loopexit(me->base_, NULL);

#ifdef PRINT_DEBUG
    printf("Slave thread stop\n");
#endif
  }
}


/**
 * Create worker thread
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
}


/**
 * Thread start function
 */
void* MyLibevent::worker_libevent(void *arg) {
  LIBEVENT_THREAD *me = (LIBEVENT_THREAD*)arg;

#ifdef PRINT_DEBUG
  printf("Worker thread %lu started\n", me->tid_);
#endif

  pthread_mutex_lock(&me->ml_ptr_->init_lock_);
  ++ me->ml_ptr_->threads_init_num_;
  pthread_cond_signal(&me->ml_ptr_->init_cond_);
  pthread_mutex_unlock(&me->ml_ptr_->init_lock_);
  
  event_base_dispatch(me->base_);  // slave thread start

  return NULL;
}


/**
 * Loop for master thread
 *
 * @param sockfd: socket fd
 * @param main_base: event_base of master thread
 */
void MyLibevent::master_thread_loop(int sockfd, struct event_base *main_base) {

  // Setup dispatcher thread
  dispatcher_thread_->tid_ = pthread_self();
  dispatcher_thread_->base_ = main_base;
  dispatcher_thread_->ml_ptr_ = this;

#ifdef PRINT_DEBUG
  printf("Dispatcher thread %lu started\n", dispatcher_thread_->tid_);
#endif

  struct event *main_event = event_new(dispatcher_thread_->base_, 
                                       sockfd, 
                                       EV_READ|EV_PERSIST, 
                                       accept_cb, 
                                       (void*)dispatcher_thread_);
  if(event_add(main_event, NULL) == -1) {
    perror("event_add failed");
    exit(EXIT_FAILURE);
  }
 
  struct timeval check_tv = {CHECK_PERIOD, 0};
  struct event *check_event = event_new(dispatcher_thread_->base_, 
                                        -1, EV_TIMEOUT|EV_PERSIST, 
                                        timeout_cb, (void*)dispatcher_thread_);
  if(event_add(check_event, &check_tv) == -1) {
    perror("event_add failed");
    exit(EXIT_FAILURE);
  }
  
  event_base_dispatch(dispatcher_thread_->base_);
}


/**
 * Callback function called when connection request comes.
 */
void MyLibevent::accept_cb(int fd, short what, void *arg) {
  int connfd;
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  memset(&client_addr, 0, sizeof(client_addr));

  connfd = accept(fd, (struct sockaddr*)&client_addr, &client_len);
  if(connfd == -1) {
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
      // TO DO
      return ;
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
}


/**
 * Dispatch new connection ( only called by master thread )
 *
 * @param sfd: socket fd used for conncetion
 * @param ev_flag: event flags
 * @param arg: used to transfer the pointer to master thread
 */
void MyLibevent::dispatch_conn_new(int sfd, int ev_flag, void *arg) {
  LIBEVENT_DISPATCHER_THREAD *me = (LIBEVENT_DISPATCHER_THREAD*)arg;
  char buf[1];
  int tid;

  CQ_ITEM *item = MyQueue::cqi_new(me->ml_ptr_->cqi_flist_);
  if(NULL == item) {
    close(sfd);
    fprintf(stderr, "Failed to allocate memory for connection object\n");
    return ;
  }
  item->fd_ = sfd;
  item->ev_flag_ = ev_flag;

  // worker distribution ( Round Robin )
  tid = (me->ml_ptr_->last_thread_ + 1) % me->ml_ptr_->threads_total_num_;
  me->ml_ptr_->last_thread_ = tid;
  
  LIBEVENT_THREAD *thread = me->ml_ptr_->threads_ + tid;

  // add task to connection queue
  MyQueue::cq_push(thread->conn_queue_, item);
  

  int ret = me->ml_ptr_->myfd_.fd_register(sfd);
#ifdef PRINT_DEBUG
  if(ret == 1) {
    printf("Register fd = %d success\n", sfd);
  } else {
    printf("Register fd = %d failed\n", sfd);
  }
#endif
 
  // connection notify
  buf[0] = 'c';
  if(write(thread->send_fd_, buf, 1) != 1)
    perror("Writing to thread notify pipe");
}


/**
 * Callback function called when TIMEOUT
 */
void MyLibevent::timeout_cb(int fd, short what, void *arg) {
  LIBEVENT_DISPATCHER_THREAD *me = (LIBEVENT_DISPATCHER_THREAD*)arg;

  if(what & EV_TIMEOUT) {

#ifdef PRINT_DEBUG
    printf("Close fds which are not used for long time.\n");
#endif
    time_t now = time(NULL);
    me->ml_ptr_->myfd_.fd_close_if_necessary(now);
  }
}


/**
 * Read callback
 */
void MyLibevent::readcb(struct bufferevent *bev, void *ctx) {
  LIBEVENT_THREAD *me = (LIBEVENT_THREAD *)ctx;

  struct evbuffer *input = bufferevent_get_input(bev);
  struct evbuffer *output = bufferevent_get_output(bev);
  int fd = bufferevent_getfd(bev);
  time_t now = time(NULL);

  int ret;

#ifdef PRINT_DEBUG
  printf("\nThread %lu is processing now\n", me->tid_);
#endif

  ret = me->ml_ptr_->myfd_.fd_update_visited_time(fd, now);

#ifdef PRINT_DEBUG
  if(ret == 1) {
    printf("Update fd = %d success\n", fd);
  } else {
    printf("Update fd = %d failed\n", fd);
  }
#endif
  
  evbuffer_add_buffer(output, input);
}


/**
 * Write callback
 */
void MyLibevent::writecb(struct bufferevent *bev, void *ctx) {
  // TO DO
  return ;
}


/**
 * Event callback
 */
void MyLibevent::eventcb(struct bufferevent *bev, short events, void *ctx) {
  if(events & BEV_EVENT_ERROR) {
    fprintf(stderr, "Error from bufferevent.\n");
  }
  else if(events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
    printf("Closing.\n");
    bufferevent_free(bev);
  }
  else if(events & (BEV_EVENT_TIMEOUT | BEV_EVENT_READING)) {
    fprintf(stderr, "Read timeout.\n");
  }
  else if(events & (BEV_EVENT_TIMEOUT | BEV_EVENT_WRITING)) {
    fprintf(stderr, "Write timeout.\n");
  }
}


