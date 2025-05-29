#include "timer.h"
#include "../http/httpconn.h"

Sort_timer::~Sort_timer()
{
    Timer *tmp = head;
    while(tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void Sort_timer::add_timer(Timer *timer)
{
    if(!timer)
    {
        return;
    }
    if(!head)
    {
        head = tail = timer;
        return ;
    }
    if(timer->expire_ < head->expire_)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return ;
    }
    add_timer(timer, head);
}

void Sort_timer::adjust_timer(Timer *timer)
{
    if(!timer)
    {
        return;
    }
    Timer *tmp = timer->next;
    if(!tmp || (timer->expire_ < tmp->expire_))
    {
        return;
    }
    if(timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void Sort_timer::del_timer(Timer *timer)
{
    if(!timer)
    {
        return;
    }
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}


void Sort_timer::tick()
{
    if (!head)
    {
        return;
    }
    
    time_t cur = time(NULL);
    Timer *tmp = head;
    while (tmp)
    {
        if (cur < tmp->expire_)
        {
            break;
        }
        tmp->cb_func(tmp->user_data_);
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void Sort_timer::add_timer(Timer *timer, Timer *lst_head)
{
    Timer *prev = lst_head;
    Timer *tmp = prev->next;
    while (tmp)
    {
        if (timer->expire_ < tmp->expire_)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::init(int timeslot)
{
    timeslot_ = timeslot;
}

int Utils::setnonblocking(int fd)
{
    int oldOption = fcntl(fd, F_GETFL);
    int newOption = oldOption | O_NONBLOCK;
    fcntl(fd, F_SETFL, newOption);
    return oldOption;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int mode)
{
    epoll_event event;
    event.data.fd = fd;
    
    if(mode == 1)
    {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Utils::sig_handler(int sig)
{
    int saveErrno = errno;
    int msg = sig;
    send(pipefd_[1], (char*)&msg, 1, 0);
    errno = saveErrno;
}

void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL)!=-1);
}

void Utils::timer_handler()
{
    timer_lst.tick();
    alarm(timeslot_);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::pipefd_ = 0;
int Utils::epollfd_ = 0;

class Utils;

void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::epollfd_, EPOLL_CTL_DEL, user_data->sockfd_, 0);
    assert(user_data);
    close(user_data->sockfd_);
    http_conn::user_count_--;
}