CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g

TARGET = webserver
OBJS = ./log/*.cpp ./pool/*.cpp ./timer/*.cpp \
       ./http/*.cpp ./server/*.cpp ./buffer/*.cpp \
	   ./epoll/*.cpp ./main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET)  -pthread -lmysqlclient

clean:
	rm -r $(TARGET)
