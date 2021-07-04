#include "webserver.h"

WebServer::WebServer() {
    users = new HttpConn[MAX_FD];                           // HttpConn 类对象
    char server_path[200];                                  // root 文件夹路径
    getcwd(server_path, 200);

    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    users_timer = new ClientData[MAX_FD];                  // 定时器
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);

    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string passwd, string db_name, int log_write, int opt_linger, int trig_mode, int sql_num, int thread_num, int close_log, int actor_model) {
    m_port = port;
    m_user = user;
    m_passwd = passwd;
    m_db_name = db_name;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_opt_linger = opt_linger;
    m_trig_mode = trig_mode;
    m_close_log = close_log;
    m_actor_model = actor_model;
}

void WebServer::trig_mode() {
    if (0 == m_trig_mode) {
        // LT + LT
        m_listen_trig_mode = 0;
        m_conn_trig_mode = 0;
    } else if (1 == m_trig_mode) {
        // LT + ET
        m_listen_trig_mode = 0;
        m_conn_trig_mode = 1;
    } else if (2 == m_trig_mode) {
        // ET + LT
        m_listen_trig_mode = 1;
        m_conn_trig_mode = 0;
    } else if (3 == m_trig_mode) {
        // ET + ET
        m_listen_trig_mode = 1;
        m_conn_trig_mode = 1;
    }
}

// 初始化日志
void WebServer::log_write() {
    if (0 == m_close_log) {
        if (1 == m_log_write)
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

// 初始化数据库连接池
void WebServer::sql_pool() {
    m_connPool = ConnectionPool::get_instance();
    m_connPool->init("localhost", m_user, m_passwd, m_db_name, 3306, m_sql_num, m_close_log);

    // 初始化数据库读取表
    users->init_mysql_result(m_connPool);
}

// 线程池
void WebServer::thread_pool() {
    m_pool = new ThreadPool<HttpConn>(m_actor_model, m_connPool, m_thread_num);
}

// 网络监听
void WebServer::event_listen() {
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 关闭连接
    if (0 == m_opt_linger) {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else if (1 == m_opt_linger) {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);

    // epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.add_fd(m_epollfd, m_listenfd, false, m_listen_trig_mode);
    HttpConn::m_epollfd = m_epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.set_nonblocking(m_pipefd[1]);
    utils.add_fd(m_epollfd, m_pipefd[0], false, 0);

    utils.add_sig(SIGPIPE, SIG_IGN);
    utils.add_sig(SIGALRM, utils.sig_handler, false);
    utils.add_sig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    // 工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

// 初始化定时器
void WebServer::timer(int connfd, struct sockaddr_in client_address) {
    users[connfd].init(connfd, client_address, m_root, m_conn_trig_mode, m_close_log, m_user, m_passwd, m_db_name);

    // 创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;

    UtilTimer *timer = new UtilTimer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

// 若有数据传输，则将定时器往后延迟3个单位并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(UtilTimer *timer) {
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

// 处理定时器
void WebServer::deal_timer(UtilTimer *timer, int sockfd) {
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
        utils.m_timer_lst.del_timer(timer);

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

// 处理连接数据
bool WebServer::deal_client_data() {
    // 初始化客户端连接地址
    struct sockaddr_in client_address;
    socklen_t client_addr_length = sizeof(client_address);

    if (0 == m_listen_trig_mode) {
        // 该连接分配的文件描述符
        int connfd = accept(m_listenfd, (struct sockaddr *) &client_address, &client_addr_length);
        if (connfd < 0) {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (HttpConn::m_user_count >= MAX_FD) {
            utils.show_error(connfd, "Internal webserver busy");
            LOG_ERROR("%s", "Internal webserver busy");
            return false;
        }
        // 初始化连接资源、定时器等
        timer(connfd, client_address);
    }
    else {
        while (1) {
            int connfd = accept(m_listenfd, (struct sockaddr *)& client_address, &client_addr_length);
            if (connfd < 0) {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (HttpConn::m_user_count >= MAX_FD) {
                utils.show_error(connfd, "Internal webserver busy");
                LOG_ERROR("%s", "Internal webserver busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

// 处理信号
bool WebServer::deal_with_signal(bool &timeout, bool &stop_server) {
    int ret = 0;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
        return false;
    else if (ret == 0) {
        return false;
    }
    else {
        for (int i = 0; i < ret; ++i) {
            switch (signals[i]) {
                case SIGALRM:
                    timeout = true;
                    break;
                case SIGTERM:
                    stop_server = true;
                    break;
            }
        }
    }
    return true;
}

// 处理读事件
void WebServer::deal_with_read(int sockfd) {
    // 创建定时器临时变量，将该连接对应的定时器取出来
    UtilTimer *timer = users_timer[sockfd].timer;

    // Reactor
    if (1 == m_actor_model) {
        if (timer)
            adjust_timer(timer);

        // 若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd, 0);

        while (true) {
            if (1 == users[sockfd].improv) {
                if (1 == users[sockfd].timer_flag) {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else {
        // Proactor
        if (users[sockfd].read_once()) {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            // 若监测到读事件，将该事件放入请求队列
            m_pool->append(users + sockfd);

            if (timer)
                adjust_timer(timer);
        }
        else{
            deal_timer(timer, sockfd);
        }
    }
}

// 处理写事件
void WebServer::deal_with_write(int sockfd) {
    UtilTimer *timer = users_timer[sockfd].timer;
    // Reactor
    if (1 == m_actor_model) {
        if (timer)
            adjust_timer(timer);

        m_pool->append(users + sockfd, 1);

        while (true) {
            if (1 == users[sockfd].improv) {
                if (1 == users[sockfd].timer_flag) {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else {
        // Proactor
        if (users[sockfd].write()) {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
                adjust_timer(timer);
        }
        else {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::event_loop() {
    bool timeout = false;                           // 超时默认为 False
    bool stop_server = false;

    while (!stop_server) {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if (sockfd == m_listenfd) {
                bool flag = deal_client_data();
                if (false == flag)
                    continue;
            }
            // 处理异常事件
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 服务器端关闭连接，移除对应的定时器
                UtilTimer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            // 处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
                // 接收到 SIGALRM 信号，timeout 设置为 true
                bool flag = deal_with_signal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "deal client data failure");
            }
            // 处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN) {
                deal_with_read(sockfd);
            }
            // 处理写事件
            else if (events[i].events & EPOLLOUT) {
                deal_with_write(sockfd);
            }
        }
        // 处理定时器为非必须事件，收到信号并不是立马处理,完成读写事件后，再进行处理
        if (timeout) {
            utils.timer_handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}