//
// Created by Derrors on 2021/5/9.
//

#include "lst_timer.h"
#include "../http/http_conn.h"

SortTimerLst::SortTimerLst() {
    head = NULL;
    tail = NULL;
}

SortTimerLst::~SortTimerLst() {
    UtilTimer *tmp = head;
    while (tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

// 添加定时器，内部调用私有成员 add_timer
void SortTimerLst::add_timer(UtilTimer *timer) {
    if (!timer) return;

    if (!head) {
        head = tail = timer;
        return;
    }

    // 如果新的定时器超时时间小于当前头部结点，直接将当前定时器结点作为头部结点
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    // 否则调用私有成员，调整内部结点
    add_timer(timer, head);
}

// 私有函数
void SortTimerLst::add_timer(UtilTimer *timer, UtilTimer *lst_head) {
    UtilTimer *prev = lst_head;
    UtilTimer *tmp = prev->next;

    // 遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置插入
    while (tmp) {
        if (timer->expire < tmp->expire) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    // 遍历完发现，目标定时器需要放到尾结点处
    if (!tmp) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

// 调整定时器，任务发生变化时，调整定时器在链表中的位置
void SortTimerLst::adjust_timer(UtilTimer *timer) {
    if (!timer) return;

    UtilTimer *tmp = timer->next;
    // 被调整的定时器在链表尾部或定时器超时值仍然小于下一个定时器超时值，不调整
    if (!tmp || (timer->expire < tmp->expire)) return;

    // 被调整定时器是链表头结点，将定时器取出，重新插入
    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    // 被调整定时器在内部，将定时器取出，重新插入
    else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

// 删除定时器
void SortTimerLst::del_timer(UtilTimer *timer) {
    if (!timer) return;

    // 链表中只有一个定时器，需要删除该定时器
    if ((timer == head) && (timer == tail)) {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    // 被删除的定时器为头结点
    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    // 被删除的定时器为尾结点
    if (timer == tail) {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    // 被删除的定时器在链表内部，常规链表结点删除
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

// 定时任务处理函数
void SortTimerLst::tick() {
    if (!head) return;

    // 获取当前时间
    time_t cur = time(NULL);
    UtilTimer *tmp = head;

    // 遍历定时器链表
    while (tmp) {
        if (cur < tmp->expire) break;                   // 若当前时间小于超时时间，说明此定时器未超时，跳出循环

        tmp->cb_func(tmp->user_data);                   // 当前定时器到期，则调用回调函数，执行定时事件

        // 将处理后的定时器从链表容器中删除，并重置头结点
        head = tmp->next;
        if (head) head->prev = NULL;

        delete tmp;
        tmp = head;
    }
}

void Utils::init(int time_slot) { m_time_slot = time_slot; }

// 对文件描述符设置非阻塞
int Utils::set_nonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 向内核事件表注册读事件，ET 模式，选择开启 EPOLLONESHOT
void Utils::add_fd(int epollfd, int fd, bool one_shot, int trig_mode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == trig_mode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

// 信号处理函数
void Utils::sig_handler(int sig) {
    // 为保证函数的可重入性，保留原来的 errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

// 设置信号函数
void Utils::add_sig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时以不断触发 SIGALRM 信号
void Utils::timer_handler() {
    m_timer_lst.tick();
    alarm(m_time_slot);
}

void Utils::show_error(int connfd, const char *info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

// 定时器回调函数: 从内核事件表删除事件，关闭文件描述符，释放连接资源
void cb_func(ClientData *user_data) {
    // 删除非活动连接在 socket 上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);

    assert(user_data);

    // 关闭文件描述符
    close(user_data->sockfd);
    // 减少连接数
    HttpConn::m_user_count--;
}
