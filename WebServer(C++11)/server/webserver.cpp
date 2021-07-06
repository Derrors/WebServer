/*
 * @Description  : WebServer
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#include "webserver.h"
using namespace std;

WebServer::WebServer(
        int port_, int trig_mode_, int timeout_, bool opt_linger_,
        int sql_port_, const char* sql_user_, const  char* sql_pwd_,
        const char* db_name_, int connPool_num_, int thread_num_,
        bool open_log_, int log_level_, int log_que_size_):
        port(port_), open_linger(opt_linger_), timeout(timeout_), is_close(false),
        timer(new Timers()), threadpool(new ThreadPool(thread_num_)), epoller(new Epoller())
{
    src_dir = getcwd(nullptr, 256);
    assert(src_dir);
    strncat(src_dir, "/html/", 16);
    HttpConn::user_count = 0;
    HttpConn::src_dir = src_dir;
    SqlConnPool::get_instance()->init("localhost", sql_port_, sql_user_, sql_pwd_, db_name_, connPool_num_);
    init_event_mode(trig_mode_);
    if(!init_socket()) { is_close = true;}

    if(open_log_) {
        Log::get_instance()->init(log_level_, "./LOG", ".log", log_que_size_);
        if(is_close) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, opt_linger_? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                     (listen_event & EPOLLET ? "ET": "LT"),
                     (conn_event & EPOLLET ? "ET": "LT"));
            LOG_INFO("Log level: %d", log_level_);
            LOG_INFO("Src dir: %s", HttpConn::src_dir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPool_num_, thread_num_);
        }
    }
}

WebServer::~WebServer() {
    close(listen_fd);
    is_close = true;
    free(src_dir);
    SqlConnPool::get_instance()->close_pool();
}

void WebServer::init_event_mode(int trigMode) {
    listen_event = EPOLLRDHUP;
    conn_event = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
        case 0:
            break;
        case 1:
            conn_event |= EPOLLET;
            break;
        case 2:
            listen_event |= EPOLLET;
            break;
        case 3:
            listen_event |= EPOLLET;
            conn_event |= EPOLLET;
            break;
        default:
            listen_event |= EPOLLET;
            conn_event |= EPOLLET;
            break;
    }
    HttpConn::is_ET = (conn_event & EPOLLET);
}

void WebServer::start() {
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!is_close) { LOG_INFO("========== Server start =========="); }
    while(!is_close) {
        if(timeout > 0) {
            timeMS = timer->get_next_tick();
        }
        int eventCnt = epoller->wait(timeMS);
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller->get_event_fd(i);
            uint32_t events = epoller->get_events(i);
            if(fd == listen_fd) {
                deal_listen();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users.count(fd) > 0);
                close_conn(&users[fd]);
            }
            else if(events & EPOLLIN) {
                assert(users.count(fd) > 0);
                deal_read(&users[fd]);
            }
            else if(events & EPOLLOUT) {
                assert(users.count(fd) > 0);
                deal_write(&users[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::send_error(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::close_conn(HttpConn* client) {
    assert(client);
    epoller->del_fd(client->get_fd());
    client->Close();
}

void WebServer::add_client(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users[fd].init(fd, addr);
    if(timeout > 0) {
        timer->add(fd, timeout, std::bind(&WebServer::close_conn, this, &users[fd]));
    }
    epoller->add_fd(fd, EPOLLIN | conn_event);
    set_fd_nonblock(fd);
}

void WebServer::deal_listen() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listen_fd, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { return;}
        else if(HttpConn::user_count >= MAX_FD) {
            send_error(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        add_client(fd, addr);
    } while(listen_event & EPOLLET);
}

void WebServer::deal_read(HttpConn* client) {
    assert(client);
    extent_time(client);
    threadpool->add_task(std::bind(&WebServer::on_read, this, client));
}

void WebServer::deal_write(HttpConn* client) {
    assert(client);
    extent_time(client);
    threadpool->add_task(std::bind(&WebServer::on_write, this, client));
}

void WebServer::extent_time(HttpConn* client) {
    assert(client);
    if(timeout > 0) {
        timer->adjust(client->get_fd(), timeout);
    }
}

void WebServer::on_read(HttpConn* client) {
    assert(client);
    int ret = -1;
    int read_error = 0;
    ret = client->read(&read_error);
    if(ret <= 0 && read_error != EAGAIN) {
        close_conn(client);
        return;
    }
    on_process(client);
}

void WebServer::on_process(HttpConn* client) {
    if(client->process()) {
        epoller->mod_fd(client->get_fd(), conn_event | EPOLLOUT);
    } 
    else {
        epoller->mod_fd(client->get_fd(), conn_event | EPOLLIN);
    }
}

void WebServer::on_write(HttpConn* client) {
    assert(client);
    int ret = -1;
    int write_error = 0;
    ret = client->write(&write_error);
    if(client->to_write_bytes() == 0) {
        /* 传输完成 */
        if(client->is_keep_alive()) {
            on_process(client);
            return;
        }
    }
    else if(ret < 0) {
        if(write_error == EAGAIN) {
            /* 继续传输 */
            epoller->mod_fd(client->get_fd(), conn_event | EPOLLOUT);
            return;
        }
    }
    close_conn(client);
}

/* Create listenFd */
bool WebServer::init_socket() {
    int ret;
    struct sockaddr_in addr;
    if(port > 65535 || port < 1024) {
        LOG_ERROR("Port:%d error!",  port);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    struct linger optLinger = { 0 };
    if(open_linger) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0) {
        LOG_ERROR("Create socket error!", port);
        return false;
    }

    ret = setsockopt(listen_fd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listen_fd);
        LOG_ERROR("Init linger error!", port);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listen_fd);
        return false;
    }

    ret = bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port);
        close(listen_fd);
        return false;
    }

    ret = listen(listen_fd, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port);
        close(listen_fd);
        return false;
    }
    ret = epoller->add_fd(listen_fd,  listen_event | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listen_fd);
        return false;
    }
    set_fd_nonblock(listen_fd);
    LOG_INFO("Server port:%d", port);
    return true;
}

int WebServer::set_fd_nonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}