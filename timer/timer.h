
#ifndef TIMER_H
#define TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>
#include <assert.h>
#include "../log/log.h"
#include "../http/httpconn.h"

class Timer;

struct client_data
{
    sockaddr_in address_;
    int sockfd_;
    Timer *timer;
};

class Timer
{
public:
    Timer() :prev(NULL), next(NULL) {}
    ~Timer() = default;
public:
    time_t expire_;

    void (* cb_func)(client_data *);
    client_data *user_data_;
    Timer *prev;
    Timer *next;
};

class Sort_timer
{
public:
    Sort_timer() :head(NULL), tail(NULL){}
    ~Sort_timer();

    void add_timer(Timer *timer);
    void adjust_timer(Timer *timer);
    void del_timer(Timer *timer);
    void tick();

public:
    void add_timer(Timer *timer, Timer *head);

    Timer *head;
    Timer *tail;
};

class Utils
{
public:
    Utils() {};
    ~Utils() {};

    void init(int timeslot);

    //设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int Mode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);
public:
    static int *pipefd_;
    Sort_timer timer_lst;
    static int epollfd_;
    int timeslot_;
};

void cb_func(client_data *user_data);

#endif