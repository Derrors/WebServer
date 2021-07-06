//
// Created by Derrors on 2021/7/5.
//

#ifndef BLOCK_DEQUE_H
#define BLOCK_DEQUE_H


#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t max_capacity = 1000);
    ~BlockDeque();

    void clear();
    bool empty();
    bool full();
    void Close();

    size_t size();
    size_t capacity();

    T front();
    T back();

    void push_back(const T& item);
    void push_front(const T& item);
    bool pop(T& item);
    bool pop(T& item, int timeout);
    void flush();

private:
    std::deque<T> deq;
    size_t capacity_;
    std::mutex mtx;
    bool is_close;

    std::condition_variable cond_consumer;
    std::condition_variable cond_producter;
};


template<class T>
BlockDeque<T>::BlockDeque(size_t max_capacity) :capacity_(max_capacity) {
    assert(max_capacity > 0);
    is_close = false;
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};

template<class T>
void BlockDeque<T>::Close() {
    {
        std::lock_guard<std::mutex> locker(mtx);
        deq.clear();
        is_close = true;
    }
    cond_producter.notify_all();
    cond_consumer.notify_all();
};

template<class T>
void BlockDeque<T>::flush() {
    cond_consumer.notify_one();
};

template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx);
    deq.clear();
}

template<class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx);
    return deq.front();
}

template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx);
    return deq.back();
}

template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx);
    return deq.size();
}

template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx);
    return capacity_;
}

template<class T>
void BlockDeque<T>::push_back(const T& item) {
    std::unique_lock<std::mutex> locker(mtx);
    while(deq.size() >= capacity_) {
        cond_producter.wait(locker);
    }
    deq.push_back(item);
    cond_consumer.notify_one();
}

template<class T>
void BlockDeque<T>::push_front(const T& item) {
    std::unique_lock<std::mutex> locker(mtx);
    while(deq.size() >= capacity_) {
        cond_producter.wait(locker);
    }
    deq.push_front(item);
    cond_consumer.notify_one();
}

template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx);
    return deq.empty();
}

template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx);
    return deq.size() >= capacity_;
}

template<class T>
bool BlockDeque<T>::pop(T& item) {
    std::unique_lock<std::mutex> locker(mtx);
    while(deq.empty()){
        cond_consumer.wait(locker);
        if(is_close){
            return false;
        }
    }
    item = deq.front();
    deq.pop_front();
    cond_producter.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop(T& item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx);
    while(deq.empty()){
        if(cond_consumer.wait_for(locker, std::chrono::seconds(timeout))
           == std::cv_status::timeout){
            return false;
        }
        if(is_close){
            return false;
        }
    }
    item = deq.front();
    deq.pop_front();
    cond_producter.notify_one();
    return true;
}


#endif
