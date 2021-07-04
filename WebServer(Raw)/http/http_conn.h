//
// Created by Derrors on 2021/5/8.
//

#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <map>

#include "../lock/locker.h"
#include "../sql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class HttpConn {
public:
    static const int FILENAME_LEN = 200;            // 设置文件的名 m_real_file 大小
    static const int READ_BUFFER_SIZE = 2048;       // 设置读缓冲区 m_read_buf 大小
    static const int WRITE_BUFFER_SIZE = 1024;      // 设置写缓冲区 m_write_buf 大小

    // 报文的请求方法，这里只用到 GET 和 POST
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH};
    // 主状态机的状态
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    // 报文解析的结果
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};
    // 从状态机的状态
    enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};

    static int m_epollfd;                           // epoll 文件描述符
    static int m_user_count;

    int m_state;                                    // 请求状态指示，读为 0, 写为 1
    int timer_flag;
    int improv;

    MYSQL *mysql;

private:
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];              // 读缓冲区，存储读取的请求报文数据
    char m_write_buf[WRITE_BUFFER_SIZE];            // 写缓冲区，存储发出的响应报文数据
    int m_read_idx;                                 // 读缓冲区中数据的长度
    int m_checked_idx;                              // 读缓冲区读取的位置
    int m_start_line;                               // 读缓冲区中已经解析的字符个数
    int m_write_idx;                                // 写缓冲区中数据的长度

    CHECK_STATE m_check_state;                      // 主状态机的状态
    METHOD m_method;                                // 请求方法

    // 以下为解析请求报文中对应的 6 个变量存储读取文件的名称
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;

    char *m_file_address;                           // 读取服务器上的文件地址
    struct stat m_file_stat;
    struct iovec m_iv[2];                           // io 向量机制
    int m_iv_count;                                 // io 向量数量
    int cgi;                                        // 是否启用的 POST
    char *m_string;                                 // 存储请求头数据
    int bytes_to_send;                              // 剩余发送字节数
    int bytes_have_send;                            // 已发送字节数
    char *doc_root;                                 // 网站根目录，文件夹内存放请求的资源和跳转的 html 文件

    map<string, string> m_users;
    int m_trig_mode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];


public:
    HttpConn() {}
    ~HttpConn() {}

    // 初始化套接字地址，函数内部调用私有方法 init
    void init(int sockfd, const sockaddr_in &addr, char *, int , int, string user, string passwd, string db_name);
    void close_conn(bool read_close = true);            // 关闭 http 连接
    void process();
    bool read_once();                                   // 读取浏览器端发来的全部数据
    bool write();                                       // 响应报文写入函数
    void init_mysql_result(ConnectionPool *connPool);   // 同步线程初始化数据库读取表
    sockaddr_in *get_address() { return &m_address; }

private:
    void init();
    HTTP_CODE process_read();                           // 从读缓冲区读取数据，并处理请求报文
    bool process_write(HTTP_CODE ret);                  // 向写缓冲区写入响应报文数据
    HTTP_CODE parse_request_line(char *text);           // 主状态机解析报文中的请求行数据
    HTTP_CODE parse_headers(char *text);                // 主状态机解析报文中的请求头数据
    HTTP_CODE parse_content(char *text);                // 主状态机解析报文中的请求内容
    HTTP_CODE do_request();                             // 生成响应报文

    // m_start_line 是已经解析的字符长度, get_line 用于将指针向后偏移，指向未处理的字符
    char *get_line() { return m_read_buf + m_start_line; }

    LINE_STATUS parse_line();                           // 从状态机读取一行，分析是请求报文的哪一部分
    void unmap();

    // 根据响应报文格式，生成对应 8 个部分，以下函数均由 do_request 调用
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
};

#endif
