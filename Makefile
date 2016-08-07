.PHONY: clean

CXX = g++
CXXFLAGS = -Wall -g -std=c++11 -rdynamic
BIN = server
OBJS = server.o my_socket.o my_fd.o my_queue.o my_libevent.o 

$(BIN):$(OBJS)
	$(CXX) -o $(BIN) $(OBJS) -lpthread -levent

%.o:%.cc
	$(CXX) -c $< -o $@ $(CXXFLAGS)

clean:
	rm *.o $(BIN)
