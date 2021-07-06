/*
 * @Description  : WebServer
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H


#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../epoll/epoller.h"
#include "../log/log.h"
#include "../timer/timer.h"
#include "../pool/sql_connection_pool.h"
#include "../pool/thread_pool.h"
#include "../http/http_connect.h"

class WebServer {
public:
    WebServer(
            int port_, int trig_mode_, int timeout_, bool opt_linger_,
            int sql_port_, const char* sql_user_, const  char* sql_pwd_,
            const char* db_name_, int connPool_num_, int thread_num_,
            bool open_log_, int log_level_, int log_que_size_);

    ~WebServer();
    void start();

private:
    bool init_socket();
    void init_event_mode(int trig_mode_);
    void add_client(int fd, sockaddr_in addr);

    void deal_listen();
    void deal_write(HttpConn* client);
    void deal_read(HttpConn* client);

    void send_error(int fd, const char*info);
    void extent_time(HttpConn* client);
    void close_conn(HttpConn* client);

    void on_read(HttpConn* client);
    void on_write(HttpConn* client);
    void on_process(HttpConn* client);

    static const int MAX_FD = 65536;
    static int set_fd_nonblock(int fd);

    int port;
    bool open_linger;
    int timeout;
    bool is_close;
    int listen_fd;
    char* src_dir;

    uint32_t listen_event;
    uint32_t conn_event;

    std::unique_ptr<Timers> timer;
    std::unique_ptr<ThreadPool> threadpool;
    std::unique_ptr<Epoller> epoller;
    std::unordered_map<int, HttpConn> users;
};


#endif
