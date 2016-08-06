#ifndef _MY_SOCKET_H_
#define _MY_SOCKET_H_

#define BACKLOG 1024

namespace mysocket 
{

int socket_set_nonblock(int fd);

int socket_set_reusable(int fd);

int socket_setup(const char *ip, int port);

};

#endif  /* _MY_SOCKET_H_ */
