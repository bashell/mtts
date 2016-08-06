#ifndef _MY_MUTEX_H_
#define _MY_MUTEX_H_

#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#define ERR_EXIT(m) \
  do { \
    perror(m);\
    exit(EXIT_FAILURE);\
  } while(0)

class MutexLockGuard;

class MutexLock {
  friend class MutexLockGuard;

 public:
  MutexLock();
  ~MutexLock();
  bool isLocked() const;
  pthread_mutex_t* getMutexPtr();
 
 private:
  MutexLock(const MutexLock&) = delete;
  MutexLock &operator=(const MutexLock&) = delete;
  
  void lock();
  void unlock();
 
 private:
  pthread_mutex_t mutex_;
  bool isLocked_;
};


class MutexLockGuard {
 public:
  MutexLockGuard(MutexLock &mutex);
  ~MutexLockGuard();              

 private:
  MutexLockGuard(const MutexLockGuard&) = delete;
  MutexLockGuard &operator=(const MutexLockGuard&) = delete;
  
 private:
  MutexLock &mutex_;
};


inline MutexLock::MutexLock() : isLocked_(false) {
  if(pthread_mutex_init(&mutex_, NULL) != 0)
    ERR_EXIT("pthread_mutex_init");
}

inline MutexLock::~MutexLock() {
  assert(isLocked_ == false);
  pthread_mutex_destroy(&mutex_);
}

inline bool MutexLock::isLocked() const {
  return isLocked_;
}

inline pthread_mutex_t* MutexLock::getMutexPtr() {
  return &mutex_;
}

inline void MutexLock::lock() {
  pthread_mutex_lock(&mutex_);
  isLocked_ = true;
}

inline void MutexLock::unlock() {
  isLocked_ = false;
  pthread_mutex_unlock(&mutex_);
}


inline MutexLockGuard::MutexLockGuard(MutexLock &mut)
    : mutex_(mut) {
  mutex_.lock();    
}

inline MutexLockGuard::~MutexLockGuard() {
  mutex_.unlock();
}


#define MutexLockGuard(m) "ERROR"

#endif  /* _MY_MUTEX_H_ */
