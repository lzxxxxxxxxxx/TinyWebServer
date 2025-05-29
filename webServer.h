#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <string>
#include <string.h>

#include "./threadpool/threadPool.h"
#include "./http/httpconn.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class WebServer
{
public:
WebServer();
~WebServer();

    void init(int port , std::string user, std::string passWord, std::string databaseName,
            int log_write , int opt_linger, int trigmode, int sql_num,
            int thread_num, int close_log, int actor_model);

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(Timer *timer);
    void deal_timer(Timer *timer, int sockfd);
    bool dealclientdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);
public:
    int port_;
    char *root_;
    int log_write_;
    int close_log_;
    int actormodel_;
    int pipefd_[2];
    int epollfd_;;
    http_conn *users_;

    ConnectionPool *connPool_;
    std::string user_; //数据库用户
    std::string passWord_; //数据库密码
    std::string databaseName_; //数据库名字
    int sql_num_; //数据库池数量

    ThreadPool<http_conn> *pool_; //线程池指针
    int thread_num_; //线程池线程数量

    epoll_event events_[MAX_EVENT_NUMBER];
    int listenfd_;
    int OPT_LINGER_;
    int TRIGMode_; //触发模式
    int listenTrigMode_; //监听socket触发模式
    int connectTrigMode_; //连接socket触发模式

    client_data *users_timer_; //定时器
    Utils utils_; //工具类，信号管理等
};

#endif