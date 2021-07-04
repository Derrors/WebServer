#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
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

#include <time.h>
#include "../log/log.h"

class UtilTimer;                                    // 连接资源结构体成员需要用到定时器类, 需要前向声明

// 连接资源
struct ClientData {
    sockaddr_in address;                            // 客户端 socket 地址
    int sockfd;                                     // socket 文件描述符
    UtilTimer *timer;                               // 定时器
};

// 定时器类
class UtilTimer {
public:
    time_t expire;                                  // 超时时间
    void (* cb_func) (ClientData *);                // 回调函数

    ClientData *user_data;                          // 连接资源
    UtilTimer *prev;                                // 前向定时器
    UtilTimer *next;                                // 后继定时器

    UtilTimer() : prev(NULL), next(NULL) {}
};

// 定时器容器类
class SortTimerLst {
private:
    UtilTimer *head;
    UtilTimer *tail;

    // 私有成员，被公有成员 add_timer 和 adjust_time 调用, 主要用于调整链表内部结点
    void add_timer(UtilTimer *timer, UtilTimer *lst_head);

public:
    SortTimerLst();
    ~SortTimerLst();

    void add_timer(UtilTimer *timer);
    void adjust_timer(UtilTimer *timer);
    void del_timer(UtilTimer *timer);
    void tick();
};

class Utils {
public:
    static int *u_pipefd;
    static int u_epollfd;
    SortTimerLst m_timer_lst;
    int m_time_slot;

    Utils() {}
    ~Utils() {}

    void init(int time_slot);

    // 对文件描述符设置非阻塞
    int set_nonblocking(int fd);

    // 向内核事件表注册读事件，ET 模式，选择开启 EPOLLONESHOT
    void add_fd(int epollfd, int fd, bool one_shot, int trig_mode);

    // 信号处理函数
    static void sig_handler(int sig);

    // 设置信号函数
    void add_sig(int sig, void (handler) (int), bool restart = true);

    // 定时处理任务，重新定时以不断触发 SIGALRM 信号
    void timer_handler();

    void show_error(int connfd, const char *info);
};

void cb_func(ClientData *user_data);

#endif
