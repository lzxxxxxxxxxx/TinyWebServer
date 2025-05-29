#ifndef LOCKER_H
#define LOCKER_H

#include <semaphore.h>
#include <exception>
#include <pthread.h>

class sem
{
public:
    sem()
    {
        if(sem_init(&sem_, 0, 0) != 0)
        {
            throw std::exception();
        }
    }

    sem(int num)
    {
        if(sem_init(&sem_, 0, num) != 0)
        {
            throw std::exception();
        }
    }

    ~sem()
    {
        sem_destroy(&sem_);
    }

    bool wait()
    {
        return sem_wait(&sem_) == 0;
    }

    bool post()
    {
        return sem_post(&sem_) == 0;
    }
private:
    sem_t sem_;
};

class locker
{
public:
    locker()
    {
        if(pthread_mutex_init(&mutex_, NULL) != 0)
        {
            throw std::exception();
        }
    }

    ~locker()
    {
        pthread_mutex_destroy(&mutex_);
    }

    bool lock()
    {
        return pthread_mutex_lock(&mutex_) == 0;
    }

    bool unlock()
    {
        return pthread_mutex_unlock(&mutex_) == 0;
    }

    pthread_mutex_t *get()
    {
        return &mutex_;
    }
private:
    pthread_mutex_t mutex_;
};

class cond
{
public:
    cond()
    {
        if(pthread_cond_init(&cond_, NULL) != 0)
        {
            throw std::exception();
        }
    }

    ~cond()
    {
        pthread_cond_destroy(&cond_);
    }

    bool wait(pthread_mutex_t *cond_mutex)
    {
        int ret = 0;
        ret = pthread_cond_wait(&cond_, cond_mutex);
        return ret == 0;
    }

    bool timewait(pthread_mutex_t *cond_mutex, struct timespec timeout)
    {
        int ret = 0;
        ret = pthread_cond_timedwait(&cond_, cond_mutex, &timeout);
        return ret == 0;
    }

    bool signal()
    {
        return pthread_cond_signal(&cond_) == 0;
    }

    bool broadcast()
    {
        return pthread_cond_broadcast(&cond_) == 0;
    }
private:
    pthread_cond_t cond_;
};

#endif