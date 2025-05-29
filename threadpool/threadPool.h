
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <stdio.h>
#include <exception>
#include <pthread.h>
#include "../mysql/connectionPool.h"


template <typename T>
class ThreadPool
{
public:
    ThreadPool(int model, ConnectionPool *connPool, int threadNum = 8, int maxRequest = 1000);
    ~ThreadPool();
    bool appendReactor(T *request, int state);
    bool appendProactor(T *request);
private:
    static void *worker(void *arg);
    void run();

    int maxThreadNum_; //线程最大数量
    int maxRequest_; //最大任务数量
    pthread_t *threads_;
    std::list<T*> workqueue_; //工作队列
    locker queuelocker_; //任务互斥锁
    sem semRequest_; //任务信号量
    ConnectionPool *connPool_;
    int model_;
};

template <typename T>
ThreadPool<T>::ThreadPool(int model, ConnectionPool *connPool, int threadNum, int maxRequest )
                            :model_(model)
                            ,connPool_(connPool)
                            ,maxThreadNum_(threadNum)
                            ,maxRequest_(maxRequest)
                            ,threads_(NULL)
{
    if(threadNum <= 0 || maxRequest <= 0)
    {
        throw std::exception();
    }
    //创建线程ID数组
    threads_ = new pthread_t[maxThreadNum_];
    if(!threads_){
        throw std::exception();
    }
    
    for(int i = 0;i < threadNum;i++)
    {
        //创建线程， 入口为worker，传入this可以用成员变量
        if(pthread_create(threads_ + i, NULL, worker, this) != 0)
        {
            delete[] threads_;
            throw std::exception();
        }
        //设置分离线程，结束后自动释放资源
        if(pthread_detach(threads_[i]))
        {
            delete[] threads_;
            throw std::exception();
        }
    }
}
template <typename T>
ThreadPool<T>::~ThreadPool()
{
    delete[] threads_;
}

template <typename T>
bool ThreadPool<T>::appendReactor(T *request, int state)
{
    queuelocker_.lock();
    if(workqueue_.size() >= maxRequest_)
    {
        queuelocker_.unlock();
        return false;
    }
    request->state_ = state;
    workqueue_.push_back(request);
    queuelocker_.unlock();
    semRequest_.post();
    return true;
}

template <typename T>
bool ThreadPool<T>::appendProactor(T *request)
{
    queuelocker_.lock();
    if(workqueue_.size() >= maxRequest_)
    {
        queuelocker_.unlock();
        return false;
    }
    workqueue_.push_back(request);
    queuelocker_.unlock();
    semRequest_.post();
    return true;
}

template <typename T>
void* ThreadPool<T>::worker(void *arg)
{
    ThreadPool* pool = (ThreadPool*) arg;
    pool->run();
    return pool;
}

template <typename T>
void ThreadPool<T>::run()
{
    while(true)
    {
        semRequest_.wait();
        queuelocker_.lock();
        if(workqueue_.empty())
        {
            queuelocker_.unlock();
            continue;
        }
        T *request = workqueue_.front();
        workqueue_.pop_front();
        queuelocker_.unlock();
        if(!request)continue;
        if(model_ == 1)
        {
            if(request->state_ == 0)
            {
                if(request->read_once())
                {
                    request->improv = 1;
                    ConnectionRAII mysqlcon(&request->mysql_, connPool_);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            ConnectionRAII mysqlcon(&request->mysql_, connPool_);
            request->process();
        }
    }
}
#endif