# mtts
*Multi-Thread Tcp Server based on Libevent.*

## Features
- *Libevent + Multi-thread.* The programming model is inspired from `Memcached`.
- *Half Sync/Half Async.* Master thread is asynchronous and responsible for listening connection request from socket. Synchronous slave threads are scheduled (by Round Robin) to execute task obtained from connection-queue or stop running when message comes from the pipe notified by master thread. 
- *Thread safe.* Each slave thread holds its own *event_base* so that the situation of multiple threads accessing only one event_base is not the case.

## Usage
```bash
$ git clone https://github.com/bashell/mtts.git
$ cd mtts
$ make
$ ./server IP PORT
```
