
#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"

template <class T>
class block_queue
{
public:
    block_queue(int maxSize = 1000)
    {
        if(maxSize <= 0)
        {
            exit(-1);
        }
        maxSize_ = maxSize;
        array_ = new T[maxSize_];
        size_ = 0;
        front_ = -1;
        back_ = -1;
    }

    void clear(){
        mutex_.lock();
        size_ = 0;
        front_ = -1;
        back_ = -1;
        mutex_.unlock();
    }

    ~block_queue()
    {
        mutex_.lock();
        if(array_ != NULL)
        {
            delete[] array_;
        }
        mutex_.unlock();
    }

    bool full()
    {
        mutex_.lock();
        if(size_ >= maxSize_)
        {
            mutex_.unlock();
            return true;
        }
        mutex_.unlock();
        return false;
    }

    bool empty()
    {
        mutex_.lock();
        if(size_ == 0)
        {
            mutex_.unlock();
            return true;
        }
        mutex_.unlock();
        return false;
    }

    bool front(T &value)
    {
        mutex_.lock();
        if(size_ == 0)
        {
            mutex_.unlock();
            return false;
        }
        value = array_[front_];
        mutex_.unlock();
        return true;
    }

    bool back(T &value)
    {
        mutex_.lock();
        if(size_ == 0)
        {
            mutex_.unlock();
            return false;
        }
        value = array_[back_];
        mutex_.unlock();
        return true;
    }

    int size()
    {
        int tmp = 0;
        mutex_.lock();
        tmp = size_;
        mutex_.unlock();
        return tmp;
    }

    int max_size()
    {
        int tmp = 0;
        mutex_.lock();
        tmp = maxSize_;
        mutex_.unlock();
        return tmp;
    }

    bool push(const T &item)
    {
        mutex_.lock();
        if(size_ >= maxSize_)
        {
            cond_.broadcast();
            mutex_.unlock();
            return false;
        }

        back_ = (back_ + 1)%maxSize_;
        array_[back_] = item;
        size_++;
        cond_.broadcast();
        mutex_.unlock();
        return true;
    }

    bool pop(T &item)
    {
        mutex_.lock();
        while(size_ <= 0)
        {
            if(!cond_.wait(mutex_.get()))
            {
                mutex_.unlock();
                return false;
            }
        }
        front_ = (front_ + 1) % maxSize_;
        item = array_[front_];
        size_--;
        mutex_.unlock();
        return true;
    }

    bool pop(T &item, int timeout)
    {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        mutex_.lock();
        if(size_ <= 0)
        {
            t.tv_sec = now.tv_sec + timeout / 1000;
            t.tv_nsec = (timeout % 1000) * 1000;
            if(!cond_.wait(mutex_.get()))
            {
                mutex_.unlock();
                return false;
            }
        }

        if(size_ <= 0)
        {
            mutex_.unlock();
            return false;
        }

        front_ = (front_ + 1) % maxSize_;
        item = array_[front_];
        size_--;
        mutex_.unlock();
        return true;
    }
private:
    locker mutex_;
    cond cond_;

    T *array_;
    int size_;
    int maxSize_;
    int front_;
    int back_;
};

#endif