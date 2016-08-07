#include <libgen.h>
#include "my_libevent.h"

int main(int argc, char *argv[]) {
  if(argc != 3) {
    fprintf(stderr, "Usage: %s IP PORT\n", basename(argv[0]));
    exit(EXIT_FAILURE);
  }
  const char *ip = argv[1];
  int port = atoi(argv[2]);
  
  int sockfd = mysocket::socket_setup(ip, port);
  struct event_base *main_base = event_base_new();
  if(NULL == main_base) {
    fprintf(stderr, "Failed to construct event_base");
    exit(EXIT_FAILURE);
  }
  MyLibevent ml;
  ml.server_start(sockfd, main_base);
  
  return 0;
}
