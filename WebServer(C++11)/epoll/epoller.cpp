/*
 * @Description  : Epoll 封装实现
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#include "epoller.h"

Epoller::Epoller(int max_event) : epoll_fd(epoll_create(512)), events(max_event){
    assert(epoll_fd >= 0 && events.size() > 0);
}

Epoller::~Epoller() {
    close(epoll_fd);
}

bool Epoller::add_fd(int fd, uint32_t events_) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events_;
    return 0 == epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::mod_fd(int fd, uint32_t events_) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events_;
    return 0 == epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::del_fd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::wait(int timeout) {
    return epoll_wait(epoll_fd, &events[0], static_cast<int>(events.size()), timeout);
}

int Epoller::get_event_fd(size_t i) const {
    assert(i < events.size() && i >= 0);
    return events[i].data.fd;
}

uint32_t Epoller::get_events(size_t i) const {
    assert(i < events.size() && i >= 0);
    return events[i].events;
}