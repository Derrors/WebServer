/*
 * @Description  : 定时器
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-07 09:00:32
 */

#ifndef TIMER_H
#define TIMER_H


#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

/* 定义定时器 */
struct TimerNode {
    int id;                                                             // 定时器编号
    TimeStamp expires;                                                  // 超时时间
    TimeoutCallBack cb;                                                 // 回调函数
    bool operator<(const TimerNode& t) {                                // 重载比较符号
        return expires < t.expires;
    }
};

class Timers {
public:
    Timers() { timer_heap.reserve(64); }
    ~Timers() { clear(); }

    void adjust(int id, int new_expires);
    void add(int id, int timeout, const TimeoutCallBack& cb);
    void clear();
    void tick();
    int get_next_tick();

private:
    void del(size_t i);
    void sift_up(size_t i);
    bool sift_down(size_t index, size_t n);
    void swap_timer(size_t i, size_t j);

    std::vector<TimerNode> timer_heap;
    std::unordered_map<int, size_t> timer_map;
};


#endif //TIMER_H
