CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp  ./timer/timer.cpp ./http/httpconn.cpp ./log/log.cpp ./mysql/connectionPool.cpp  webServer.cpp config.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r server