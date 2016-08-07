#include <stdio.h>
#include <stdlib.h>
#include "my_queue.h"

MyQueue::MyQueue() {}

MyQueue::~MyQueue() {}

/**
 * Initialize connection queue.
 *
 * @param cq: pointer to the connection queue
 */
void MyQueue::cq_init(CQ *cq) {
  int res;
  if((res = pthread_mutex_init(&cq->lock_, NULL)) != 0) {
    perror("pthread_mutex_init");
    exit(EXIT_FAILURE);
  }
  cq->head_ = NULL;
  cq->tail_ = NULL;
}

/**
 * Initialize connection queue freelist.
 *
 * @param cqi_flist: pointer to freelist
 */
void MyQueue::cqi_flist_init(CQ_ITEM_FLIST *cqi_flist) {
  int res;
  if((res = pthread_mutex_init(&cqi_flist->cqi_freelist_lock_, NULL)) != 0) {
    perror("pthread_mutex_init");
    exit(EXIT_FAILURE);
  }
  cqi_flist->cqi_freelist_ = NULL;
}

/**
 * Get a connection queue item from freelist.
 *
 * @param cqi_flist: pointer to freelist
 * @return a fresh connection queue item
 */
CQ_ITEM* MyQueue::cqi_new(CQ_ITEM_FLIST *cqi_flist) {
  CQ_ITEM *item = NULL;
  pthread_mutex_lock(&cqi_flist->cqi_freelist_lock_);
  if(cqi_flist->cqi_freelist_) {
    item = cqi_flist->cqi_freelist_;
    cqi_flist->cqi_freelist_ = item->next_;
  }
  pthread_mutex_unlock(&cqi_flist->cqi_freelist_lock_);
   
  if(NULL == item) {
    int i;

    // Allocate a bunch of items at once to reduce fragmentation
    item = (CQ_ITEM*)malloc(ITEMS_PER_ALLOC * sizeof(CQ_ITEM));
    if(NULL == item) {
      perror("Failed to allocate memory for item.");
      return NULL;
    }

    // Link together all the new items except the first one
    for(i = 2; i < ITEMS_PER_ALLOC; ++i) {
      item[i-1].next_ = &item[i];
    }

    // Link the last one to the second one (the first one will be returned)
    pthread_mutex_lock(&cqi_flist->cqi_freelist_lock_);
    item[ITEMS_PER_ALLOC-1].next_ = cqi_flist->cqi_freelist_;
    cqi_flist->cqi_freelist_ = &item[1];
    pthread_mutex_unlock(&cqi_flist->cqi_freelist_lock_);
  }

  return item;
}

/**
 * Frees a connection queue item (adds it to the freelist).
 *
 * @param item: the connection queue item to be freed
 * @param cqi_flist: pointer to freelist
 */
void MyQueue::cqi_free(CQ_ITEM *item, CQ_ITEM_FLIST *cqi_flist) {
  pthread_mutex_lock(&cqi_flist->cqi_freelist_lock_);
  item->next_ = cqi_flist->cqi_freelist_;
  cqi_flist->cqi_freelist_ = item;
  pthread_mutex_unlock(&cqi_flist->cqi_freelist_lock_);
}

/**
 * Push an item to connection queue.
 *
 * @param cq: pointer to connection queue
 * @param item: pointer to the item to be added
 */
void MyQueue::cq_push(CQ *cq, CQ_ITEM *item) {
  item->next_ = NULL;

  pthread_mutex_lock(&cq->lock_);
  if(NULL == cq->tail_)
    cq->head_ = item;
  else
    cq->tail_->next_ = item;
  
  cq->tail_ = item;
  pthread_mutex_unlock(&cq->lock_);
}


/**
 * Looks for an item on a connection queue, but doesn't block if there isn't one.
 *
 * @param cq: pointer to a connection queue
 * @return an item, or NULL if no item is available
 */
CQ_ITEM* MyQueue::cq_pop(CQ *cq) {
  CQ_ITEM *item;

  pthread_mutex_lock(&cq->lock_);
  item = cq->head_;  // get the head
  if(NULL != item) {
    cq->head_ = item->next_;
    if(NULL == cq->head_)
      cq->tail_ = NULL;
  }
  pthread_mutex_unlock(&cq->lock_);
  
  return item;
}

