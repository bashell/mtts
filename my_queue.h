#ifndef _MY_QUEUE_H_
#define _MY_QUEUE_H_

#include <pthread.h>

#define ITEMS_PER_ALLOC 64


/* item in connection queue */
typedef struct conn_queue_item CQ_ITEM;
struct conn_queue_item {
  int fd_;
  int ev_flag_;
  CQ_ITEM *next_;
};

/* connection queue */
typedef struct conn_queue CQ;
struct conn_queue {
  CQ_ITEM *head_;
  CQ_ITEM *tail_;
  pthread_mutex_t lock_;
};

/* freelist of connection queue */
typedef struct conn_queue_item_freelist CQ_ITEM_FLIST;
struct conn_queue_item_freelist {
  CQ_ITEM *cqi_freelist_;
  pthread_mutex_t cqi_freelist_lock_;
};


class MyLibevent;

class MyQueue {
  friend class MyLibevent;

 public:
  MyQueue();
  ~MyQueue();

 private:
  static void cq_init(CQ *cq);
  static void cqi_flist_init(CQ_ITEM_FLIST *cqi_flist);
  static void cq_push(CQ *cq, CQ_ITEM *item);
  static CQ_ITEM *cq_pop(CQ *cq);
  static CQ_ITEM *cqi_new(CQ_ITEM_FLIST *cqi_flist);
  static void cqi_free(CQ_ITEM *item, CQ_ITEM_FLIST *cqi_flist);

 private:
  MyQueue(const MyQueue&) = delete;
  MyQueue &operator=(const MyQueue&) = delete;
};


#endif  /* _MY_QUEUE_H_ */
