// 信号量、锁、条件变量的类定义
// Created by Derrors on 2021/5/7.
//

#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>


class Sem {
private:
    sem_t m_sem;        // 信号量

public:
    // 信号量的构造、析构函数
    Sem() { if (sem_init(&m_sem, 0, 0)) throw std::exception(); }

    Sem(int num) { if (sem_init(&m_sem, 0, num)) throw std::exception(); }

    ~Sem() { sem_destroy(&m_sem); }

    // 信号量 -1, 信号量为 0 时, sem_wait 阻塞直到信号量大于 0
    bool wait() { return sem_wait(&m_sem) == 0; }

    // 信号量 +1
    bool post() { return sem_post(&m_sem) == 0; }
};

class Locker {
private:
    pthread_mutex_t m_mutex;        // 互斥锁

public:
    Locker() { if(pthread_mutex_init(&m_mutex, NULL)) throw std::exception(); }

    ~Locker() { pthread_mutex_destroy(&m_mutex); }

    // 加锁，锁被占用时挂起等待
    bool lock() { return pthread_mutex_lock(&m_mutex) == 0; }

    // 解锁
    bool unlock() { return pthread_mutex_unlock(&m_mutex) == 0; }

    // 获取锁
    pthread_mutex_t *get() { return &m_mutex; }
};

class Cond {
private:
    pthread_cond_t m_cond;

public:
    Cond() { if(pthread_cond_init(&m_cond, NULL)) throw std::exception(); }
    ~Cond() { pthread_cond_destroy(&m_cond); }

    // 无条件等待 m_cond 成立
    bool wait(pthread_mutex_t *m_mutex) { return pthread_cond_wait(&m_cond, m_mutex) == 0; }

    // 计时等待
    bool time_wait(pthread_mutex_t *m_mutex, struct timespec t) { return pthread_cond_timedwait(&m_cond, m_mutex, &t); }

    // 按入队顺序激活其中一个等待 m_cond 的线程
    bool signal() { return pthread_cond_signal(&m_cond) == 0; }

    // 以广播的方式激活所有等待 m_cond 的线程
    bool broadcast() { return pthread_cond_broadcast(&m_cond) == 0; }
};

#endif
