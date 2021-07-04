//
// Created by Derrors on 2021/5/8.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include "../lock/locker.h"
#include "../sql/sql_connection_pool.h"

template <typename T>
class ThreadPool {
private:
    int m_actor_model;                                  // 模型切换
    int m_thread_number;                                // 线程池中的线程数
    int m_max_requests;                                 // 请求队列中允许的最大请求数
    pthread_t *m_threads;                               // 线程池的数组，其大小为 m_thread_number
    std::list<T *> m_worker_queue;                      // 请求队列
    Locker m_queue_locker;                              // 请求队列的互斥锁
    Sem m_queue_stat;                                   // 请求队列状态，是否有任务需要处理
    ConnectionPool *m_connPool;                         // 数据库连接池

    // 工作线程的运行函数，它不断从工作队列中取出任务并执行
    static void *worker(void *arg);
    void run();

public:
    ThreadPool(int actor_model, ConnectionPool *connPool, int thread_number = 8, int max_request = 10000);
    ~ThreadPool();

    // 向请求队列中插入任务请求
    bool append(T *request, int state);
    bool append(T *request);
};

template <typename T>
ThreadPool<T>::ThreadPool(int actor_model, ConnectionPool *connPool, int thread_number, int max_request) : m_actor_model(actor_model), m_connPool(connPool), m_thread_number(thread_number), m_max_requests(max_request), m_threads(NULL) {
    if(thread_number <= 0 || max_request <= 0) throw std::exception();

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) throw std::exception();

    for(int i = 0; i < thread_number; ++i) {
        // 循环创建线程，并将工作线程指定运行函数
        if(pthread_create(m_threads + i, NULL, worker, this)) {
            delete[] m_threads;
            throw std::exception();
        }

        // 主线程与子线程分离，子线程结束后，资源自动回收
        if(pthread_detach(m_threads[i])) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool() { delete[] m_threads; }

template <typename T>
bool ThreadPool<T>::append(T *request, int state) {
    // 对请求队列加锁
    m_queue_locker.lock();

    // 查看请求队列是否已满
    if(m_worker_queue.size() >= m_max_requests) {
        m_queue_locker.unlock();
        return false;
    }

    request->m_state = state;
    m_worker_queue.push_back(request);      // 添加请求
    m_queue_locker.unlock();                // 解锁
    m_queue_stat.post();                    // 通知工作线程有任务需要处理

    return true;
}

template <typename T>
bool ThreadPool<T>::append(T *request) {
    m_queue_locker.lock();

    if(m_worker_queue.size() >= m_max_requests) {
        m_queue_locker.unlock();
        return false;
    }

    m_worker_queue.push_back(request);
    m_queue_locker.unlock();
    m_queue_stat.post();

    return true;
}

template<typename T>
void *ThreadPool<T>::worker(void *arg) {
    // 将参数强转为线程池类，调用成员方法
    ThreadPool *pool = (ThreadPool *) arg;
    pool->run();
    return pool;
}


template <typename T>
void ThreadPool<T>::run() {
    while(true) {
        m_queue_stat.wait();            // 等待请求队列中插入新的任务
        m_queue_locker.lock();          // 加锁

        if(m_worker_queue.empty()) {
            m_queue_locker.unlock();
            continue;
        }

        // 从请求队列中取出第一个任务，并将其在队列中移除，解锁队列
        T *request = m_worker_queue.front();
        m_worker_queue.pop_front();
        m_queue_locker.unlock();

        if(!request) continue;

        // 对请求任务进行处理
        if(1 == m_actor_model) {
            if(0 == request->m_state) {
                if(request->read_once()) {
                    request->improv = 1;
                    ConnectionRAII mysql_conn(&request->mysql, m_connPool);
                    request->process();
                }
                else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else {
                if(request->write())
                    request->improv = 1;
                else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else {
            ConnectionRAII mysql_conn(&request->mysql, m_connPool);
            request->process();
        }
    }
}

#endif
