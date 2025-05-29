
#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

#define MAX_NAME_LEN 256
#define LOG_BUF_SIZE 8192
#define MAX_LINES 5000000


class Log
{
public:
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    static void *flush_log(void *argc)
    {
        Log::get_instance()->async_write_log();
        return NULL;
    }

    bool init(const char *fileName, int close_log, 
        int bufSize = LOG_BUF_SIZE, int maxLines = MAX_LINES, int maxQueueSize = 0);

    void write_log(int level, const char *format, ...);

    void flush(void);

    
private:
    Log();
    virtual ~Log();
    void *async_write_log()
    {
        std::string single_log;
        while(queueLog_->pop(single_log))
        {
            mutexLog_.lock();
            fputs(single_log.c_str(), fp_);
            mutexLog_.unlock();
        }
        return NULL;
    }

    char pathName_[MAX_NAME_LEN]; // 路径名
    char fileName_[MAX_NAME_LEN]; //文件名
    int maxLines_; //日志最大行数
    int bufSize_; //缓冲区大小
    int count_; //每日日志数
    int today_; //记录当天是那一天
    FILE *fp_; //打开log文件
    char *buf_; //缓冲区
    block_queue<std::string> *queueLog_; //阻塞队列，异步使用
    bool is_async_; //是否为同步
    locker mutexLog_; //互斥锁
    int close_log_; //关闭日志
};


#define LOG_DEBUG(format, ...) if(0 == close_log_) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == close_log_) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == close_log_) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == close_log_) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#endif