#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
using namespace std;

Log::Log()
{
    count_ = 0;
    is_async_ = false;
}

Log::~Log()
{
    if (fp_ != NULL)
    {
        fclose(fp_);
    }
}
//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    //如果设置了max_queue_size,则设置为异步
    if (max_queue_size >= 1)
    {
        is_async_ = true;
        queueLog_ = new block_queue<string>(max_queue_size);
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, NULL, flush_log, NULL);
    }
    
    close_log_ = close_log;
    bufSize_ = log_buf_size;
    buf_ = new char[bufSize_];
    memset(buf_, '\0', bufSize_);
    maxLines_ = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

 
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    if (p == NULL)
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(fileName_, p + 1);
        strncpy(pathName_, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", pathName_, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, fileName_);
    }

    today_ = my_tm.tm_mday;
    
    fp_ = fopen(log_full_name, "a");
    if (fp_ == NULL)
    {
        return false;
    }

    return true;
}

void Log::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    //写入一个log，对m_count++, m_split_lines最大行数
    mutexLog_.lock();
    count_++;

    if (today_ != my_tm.tm_mday || count_ % maxLines_ == 0) //everyday log
    {
        
        char new_log[256] = {0};
        fflush(fp_);
        fclose(fp_);
        char tail[16] = {0};
       
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
       
        if (today_ != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", pathName_, tail, fileName_);
            today_ = my_tm.tm_mday;
            count_ = 0;
        }
        else
        {
            snprintf(new_log, 255, "%s%s%s.%d", pathName_, tail, fileName_, count_ / maxLines_);
        }
        fp_ = fopen(new_log, "a");
    }
 
    mutexLog_.unlock();

    va_list valst;
    va_start(valst, format);

    string log_str;
    mutexLog_.lock();

    //写入的具体时间内容格式
    int n = snprintf(buf_, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    
    int m = vsnprintf(buf_ + n, bufSize_ - 1, format, valst);
    buf_[n + m] = '\n';
    buf_[n + m + 1] = '\0';
    log_str = buf_;

    mutexLog_.unlock();

    if (is_async_ && !queueLog_->full())
    {
        queueLog_->push(log_str);
    }
    else
    {
        mutexLog_.lock();
        fputs(log_str.c_str(), fp_);
        mutexLog_.unlock();
    }

    va_end(valst);
}

void Log::flush(void)
{
    mutexLog_.lock();
    //强制刷新写入流缓冲区
    fflush(fp_);
    mutexLog_.unlock();
}
