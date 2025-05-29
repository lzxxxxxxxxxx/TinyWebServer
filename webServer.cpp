#include "webServer.h"

WebServer::WebServer()
{
    //http_conn类对象
    users_ = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    root_ = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(root_, server_path);
    strcat(root_, root);

    //定时器
    users_timer_ = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    close(epollfd_);
    close(listenfd_);
    close(pipefd_[1]);
    close(pipefd_[0]);
    delete[] users_;
    delete[] users_timer_;
    delete pool_;
}

void WebServer::init(int port, std::string user, std::string passWord, std::string databaseName, int log_write, 
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    port_ = port;
    user_ = user;
    passWord_ = passWord;
    databaseName_ = databaseName;
    sql_num_ = sql_num;
    thread_num_ = thread_num;
    log_write_ = log_write;
    OPT_LINGER_ = opt_linger;
    TRIGMode_ = trigmode;
    close_log_ = close_log;
    actormodel_ = actor_model;
}

void WebServer::trig_mode()
{
    //LT + LT
    if (0 == TRIGMode_)
    {
        listenTrigMode_ = 0;
        connectTrigMode_ = 0;
    }
    //LT + ET
    else if (1 == TRIGMode_)
    {
        listenTrigMode_ = 0;
        connectTrigMode_ = 1;
    }
    //ET + LT
    else if (2 == TRIGMode_)
    {
        listenTrigMode_ = 1;
        connectTrigMode_ = 0;
    }
    //ET + ET
    else if (3 == TRIGMode_)
    {
        listenTrigMode_ = 1;
        connectTrigMode_ = 1;
    }
}

void WebServer::log_write()
{
    if (0 == close_log_)
    {
        //初始化日志
        if (1 == log_write_)
            Log::get_instance()->init("./ServerLog", close_log_, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog", close_log_, 2000, 800000, 0);
    }
}

void WebServer::sql_pool()
{
    //初始化数据库连接池
    connPool_ = ConnectionPool::getInstance();
    connPool_->init("localhost", user_, passWord_, databaseName_, 3306, sql_num_, close_log_);

    //初始化数据库读取表
    users_->initmysql_result(connPool_);
}

void WebServer::thread_pool()
{
    //线程池
    pool_ = new ThreadPool<http_conn>(actormodel_, connPool_, thread_num_);
}

void WebServer::eventListen()
{
    //网络编程基础步骤
    listenfd_ = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd_ >= 0);

    //优雅关闭连接
    if (0 == OPT_LINGER_)
    {
        struct linger tmp = {0, 1};
        setsockopt(listenfd_, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == OPT_LINGER_)
    {
        struct linger tmp = {1, 1};
        setsockopt(listenfd_, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port_);

    int flag = 1;
    setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenfd_, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd_, 5);
    assert(ret >= 0);

    utils_.init(TIMESLOT);

    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd_ = epoll_create(5);
    assert(epollfd_ != -1);

    utils_.addfd(epollfd_, listenfd_, false, listenTrigMode_);
    http_conn::epollfd_ = epollfd_;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd_);
    assert(ret != -1);
    utils_.setnonblocking(pipefd_[1]);
    utils_.addfd(epollfd_, pipefd_[0], false, 0);

    utils_.addsig(SIGPIPE, SIG_IGN);
    utils_.addsig(SIGALRM, utils_.sig_handler, false);
    utils_.addsig(SIGTERM, utils_.sig_handler, false);

    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    Utils::pipefd_ = pipefd_;
    Utils::epollfd_ = epollfd_;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    users_[connfd].init(connfd, client_address, root_, connectTrigMode_, close_log_, user_, passWord_, databaseName_);

    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer_[connfd].address_ = client_address;
    users_timer_[connfd].sockfd_ = connfd;
    Timer *timer = new Timer;
    timer->user_data_ = &users_timer_[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire_ = cur + 3 * TIMESLOT;
    users_timer_[connfd].timer = timer;
    utils_.timer_lst.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(Timer *timer)
{
    time_t cur = time(NULL);
    timer->expire_ = cur + 3 * TIMESLOT;
    utils_.timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

void WebServer::deal_timer(Timer *timer, int sockfd)
{
    timer->cb_func(&users_timer_[sockfd]);
    if (timer)
    {
        utils_.timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer_[sockfd].sockfd_);
}

bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (0 == listenTrigMode_)
    {
        int connfd = accept(listenfd_, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::user_count_ >= MAX_FD)
        {
            utils_.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }

    else
    {
        while (1)
        {
            int connfd = accept(listenfd_, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::user_count_ >= MAX_FD)
            {
                utils_.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(pipefd_[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    Timer *timer = users_timer_[sockfd].timer;

    //reactor
    if (1 == actormodel_)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        //若监测到读事件，将该事件放入请求队列
        pool_->appendReactor(users_ + sockfd, 0);

        while (true)
        {
            if (1 == users_[sockfd].improv)
            {
                if (1 == users_[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users_[sockfd].timer_flag = 0;
                }
                users_[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if (users_[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users_[sockfd].get_address()->sin_addr));

            //若监测到读事件，将该事件放入请求队列
            pool_->appendProactor(users_ + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    Timer *timer = users_timer_[sockfd].timer;
    //reactor
    if (1 == actormodel_)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        pool_->appendReactor(users_ + sockfd, 1);

        while (true)
        {
            if (1 == users_[sockfd].improv)
            {
                if (1 == users_[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users_[sockfd].timer_flag = 0;
                }
                users_[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if (users_[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users_[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(epollfd_, events_, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events_[i].data.fd;

            //处理新到的客户连接
            if (sockfd == listenfd_)
            {
                bool flag = dealclientdata();
                if (false == flag)
                    continue;
            }
            else if (events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                Timer *timer = users_timer_[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //处理信号
            else if ((sockfd == pipefd_[0]) && (events_[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户连接上接收到的数据
            else if (events_[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (events_[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if (timeout)
        {
            utils_.timer_handler();

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }
}