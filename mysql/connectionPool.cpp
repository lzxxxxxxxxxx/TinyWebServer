#include "connectionPool.h"
#include "../log/log.h"

#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>


ConnectionPool::ConnectionPool()
{
    curConn_ = 0;
    freeConn_ = 0;
}

//单例模式
ConnectionPool *ConnectionPool::getInstance()
{
    static ConnectionPool connPool;
    return &connPool;
}

//初始化
void ConnectionPool::init(std::string host, std::string user, std::string passWord,
    std::string databaseName, int port, int maxConn, int closeLog)
{
    host_ = host;
    user_ = user;
    passWord_ = passWord;
    databaseName_ = databaseName;
    port_ = port;
    maxConn_ = maxConn;
    close_log_ = closeLog;

    //创建池
    for(int i = 0; i < maxConn; i++)
    {
        MYSQL *con = NULL;
        con = mysql_init(con);
        if(con == NULL)
        {
            LOG_ERROR("Mysql Error");
            exit(1);
        }
        con = mysql_real_connect(con, host.c_str(), user.c_str(), passWord.c_str(), 
            databaseName.c_str(), port, NULL, 0);
        if(con == NULL)
        {
            LOG_ERROR("Mysql Error");
            exit(1);
        }
        connlist_.push_back(con);
        ++freeConn_;
    }
    connSem_ = sem(freeConn_);
    maxConn_ = freeConn_;
}

MYSQL *ConnectionPool::getConnection()
{
    MYSQL *conn = NULL;
    if(connlist_.size() == 0)
    {
        return NULL;
    }
    connSem_.wait();
    lock_.lock();

    conn = connlist_.front();
    connlist_.pop_front();
    --freeConn_;
    ++curConn_;

    lock_.unlock();
    return conn;
}

bool ConnectionPool::ReleaseConnection(MYSQL *conn)
{
    if(conn == NULL)
    {
        return false;
    }
    lock_.lock();

    connlist_.push_back(conn);
    ++freeConn_;
    --curConn_;

    lock_.unlock();
    connSem_.post();
    return true;
}

void ConnectionPool::destroyPool()
{
    lock_.lock();

    //关闭队列里所有连接
    if (connlist_.size() > 0)
	{
		std::list<MYSQL *>::iterator it;
		for (it = connlist_.begin(); it != connlist_.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}
		curConn_ = 0;
		freeConn_ = 0;
		connlist_.clear();
	}

    lock_.unlock();
}

ConnectionPool::~ConnectionPool()
{
    destroyPool();
}

int ConnectionPool::getFreeConnection()
{
    return this->freeConn_;
}

ConnectionRAII::ConnectionRAII(MYSQL **sql, ConnectionPool *connPool)
{
    *sql = connPool->getConnection();
    connRAII_ = *sql;
    poolRAII_ = connPool;
}

ConnectionRAII::~ConnectionRAII()
{
    poolRAII_->ReleaseConnection(connRAII_);
}