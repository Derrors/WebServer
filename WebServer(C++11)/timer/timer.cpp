//
// Created by Derrors on 2021/7/5.
//

#include "timer.h"

/* 从第 i 个结点开始，向上处理堆 */
void Timers::sift_up(size_t i) {
    assert(i >= 0 && i < timer_heap.size());
    size_t j = (i - 1) / 2;                             // 父结点
    while(j >= 0) {
        if(timer_heap[j] < timer_heap[i]) { break; }
        swap_timer(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

/* 从第 i 个结点开始，向下处理堆 */
bool Timers::sift_down(size_t index, size_t n) {
    assert(index >= 0 && index < timer_heap.size());
    assert(n >= 0 && n <= timer_heap.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && timer_heap[j + 1] < timer_heap[j]) j++;         // 取左右孩子中最小的那一个
        if(timer_heap[i] < timer_heap[j]) break;
        swap_timer(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

/* 交换两个定时器 */
void Timers::swap_timer(size_t i, size_t j) {
    assert(i >= 0 && i < timer_heap.size());
    assert(j >= 0 && j < timer_heap.size());
    std::swap(timer_heap[i], timer_heap[j]);
    // 更新定时器对应的映射
    timer_map[timer_heap[i].id] = i;
    timer_map[timer_heap[j].id] = j;
}

/* 添加一个定时器，可能是新增，也可能是已有的定时器 */
void Timers::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if(timer_map.count(id) == 0) {
        /* 新节点：堆尾插入，调整堆 */
        i = timer_heap.size();
        timer_map[id] = i;
        timer_heap.push_back({id, Clock::now() + MS(timeout), cb});
        sift_up(i);
    }
    else {
        /* 已有结点：调整堆 */
        i = timer_map[id];
        timer_heap[i].expires = Clock::now() + MS(timeout);
        timer_heap[i].cb = cb;
        if(!sift_down(i, timer_heap.size())) {
            sift_up(i);
        }
    }
}

/* 第 id 个定时器的事件发生，触发回调函数并删除对应的定时器 */
void Timers::run(int id) {
    if(timer_heap.empty() || timer_map.count(id) == 0) {
        return;
    }
    /* 删除指定 id 结点，并触发回调函数 */
    size_t i = timer_map[id];
    TimerNode node = timer_heap[i];
    node.cb();
    del(i);
}

/* 删除指定位置的定时器 */
void Timers::del(size_t index) {
    assert(!timer_heap.empty() && index >= 0 && index < timer_heap.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = timer_heap.size() - 1;
    assert(i <= n);
    if(i < n) {
        swap_timer(i, n);
        if(!sift_down(i, n)) {
            sift_up(i);
        }
    }
    /* 队尾元素删除 */
    timer_map.erase(timer_heap.back().id);
    timer_heap.pop_back();
}

/* 调整指定 id 的定时器 */
void Timers::adjust(int id, int timeout) {
    assert(!timer_heap.empty() && timer_map.count(id) > 0);
    timer_heap[timer_map[id]].expires = Clock::now() + MS(timeout);;
    sift_down(timer_map[id], timer_heap.size());
}

/* 清除超时的定时器 */
void Timers::tick() {
    if(timer_heap.empty()) {
        return;
    }
    while(!timer_heap.empty()) {
        TimerNode node = timer_heap.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        del(0);
    }
}

void Timers::clear() {
    timer_heap.clear();
    timer_map.clear();
}

/* 计算下一个即将超时的定时器还需多长时间超时 */
int Timers::get_next_tick() {
    tick();
    size_t res = -1;
    if(!timer_heap.empty()) {
        res = std::chrono::duration_cast<MS>(timer_heap.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}