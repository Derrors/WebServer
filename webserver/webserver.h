#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "../threadpool/threadpool.h"
#include "../http/http_conn.h"

const int MAX_FD = 65536;                   // 最大文件描述符
const int MAX_EVENT_NUMBER = 10000;         // 最大事件数
const int TIMESLOT = 5;                     // 最小超时单位

class WebServer {
public:
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actor_model;

    int m_pipefd[2];
    int m_epollfd;
    HttpConn *users;

    // 数据库相关
    ConnectionPool *m_connPool;
    string m_user;                          // 登陆数据库用户名
    string m_passwd;                        // 登陆数据库密码
    string m_db_name;                       // 使用数据库名
    int m_sql_num;

    // 线程池相关
    ThreadPool <HttpConn> *m_pool;
    int m_thread_num;

    // epoll_event 相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_opt_linger;
    int m_trig_mode;
    int m_listen_trig_mode;
    int m_conn_trig_mode;

    // 定时器相关
    ClientData *users_timer;
    Utils utils;


    WebServer();
    ~WebServer();

    void init(int port , string user, string passwd, string db_name,
              int log_write , int opt_linger, int trig_mode, int sql_num,
              int thread_num, int close_log, int actor_model);

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void event_listen();
    void event_loop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(UtilTimer *timer);
    void deal_timer(UtilTimer *timer, int sockfd);
    bool deal_client_data();
    bool deal_with_signal(bool& timeout, bool& stop_server);
    void deal_with_read(int sockfd);
    void deal_with_write(int sockfd);

};
#endif
