/*
 * @Description  : Epoll 封装
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#ifndef EPOLLER_H
#define EPOLLER_H


#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int max_events = 1024);
    ~Epoller();

    bool add_fd(int fd, uint32_t events);
    bool mod_fd(int fd, uint32_t events);
    bool del_fd(int fd);

    int wait(int timeout = -1);
    int get_event_fd(size_t i) const;
    uint32_t get_events(size_t i) const;

private:
    int epoll_fd;
    std::vector<struct epoll_event> events;
};


#endif
