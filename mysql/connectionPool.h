
#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include <stdio.h>
#include <queue>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <iostream>
#include <list>
#include "../lock/locker.h"
// #include "../log/log.h"

class ConnectionPool
{
public:

    MYSQL *getConnection(); //获取数据库连接
    bool ReleaseConnection(MYSQL *connection); //断开连接
    int getFreeConnection(); //获取连接
    void destroyPool(); //销毁所有连接

    //单例模式
    static ConnectionPool *getInstance();

    void init(std::string host, std::string user, std::string passWord,
         std::string databaseName, int port, int maxConn, int closeLog);
    
private:
    ConnectionPool();
    ~ConnectionPool();

    int maxConn_; //最大连接数
    int curConn_; //已使用连接
    int freeConn_; //空闲连接

    locker lock_; //锁
    std::list<MYSQL*> connlist_; //连接池
    sem connSem_;

public:
    int close_log_; //日志开关
    std::string host_; //主机地址
    std::string port_; //端口号
    std::string user_; //用户名
    std::string passWord_; //数据库密码
    std::string databaseName_; //数据库名

};



class ConnectionRAII
{
public:
    ConnectionRAII(MYSQL **con, ConnectionPool *connPool);
    ~ConnectionRAII();
private:
    MYSQL *connRAII_;
    ConnectionPool *poolRAII_;
};

#endif