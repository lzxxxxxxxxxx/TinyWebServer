
#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../mysql/connectionPool.h"
#include "../timer/timer.h"
#include "../log/log.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };

    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

    http_conn(){}
    ~http_conn(){}
    
    void init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
        int close_log, std::string user, std::string passwd, std::string sqlname);
    void close_conn(bool real_close = true);
    void process();
    //一次性读数据读到缓冲区
    bool read_once();
    bool write();
    sockaddr_in *get_address() {return &address_;}
    void initmysql_result(ConnectionPool *connPool);

    int timer_flag; // 标记是否被定时器标记
    int improv; //标记是否处理异常
    
    static int epollfd_;
    static int user_count_;//当前用户连接数
    MYSQL *mysql_;
    int state_; //连接状态
private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() {return read_buf_ + start_line_;}
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger(); //添加Connection
    bool add_blank_line(); //添加空行


    int sockfd_;
    sockaddr_in address_;
    char read_buf_[READ_BUFFER_SIZE];
    long read_idx_;
    long checked_idx_; //当前正在解析字符位置
    int start_line_;
    char write_buf_[WRITE_BUFFER_SIZE];
    int write_idx_;
    CHECK_STATE check_state_;
    METHOD method_; //GET or POST
    char real_file_[FILENAME_LEN]; //请求文件完整路径
    char *url_; //请求的url
    char *version_; //HTTP版本
    char *host_; //请求主机名
    long content_length_; 
    bool linger_; //是否保持连接
    char *file_address_; //内存映射的文件地址
    struct stat file_stat_; //文件状态信息
    struct iovec iv[2];
    int iv_count_; //分散写的块数
    int cgi_; //是否启用cgi
    char *string_;
    int bytes_to_send_; //剩余待发送数据
    int bytes_have_send_; //已发送数据
    char *doc_root_; //网站根目录

    std::map<std::string, std::string> users_; //存储用户账号，密码
    int TRIGMode_; //触发模式
    int close_log_; //是否关闭日志

    char sql_user_[100]; //数据用户
    char sql_passwd_[100]; //数据库密码
    char sql_name_[100]; //数据库名

};

#endif