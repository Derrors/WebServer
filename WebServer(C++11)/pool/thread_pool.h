/*
 * @Description  : 线程池
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>

class ThreadPool {
public:
    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;

    /* 构造函数，创建子线程并分离运行 */
    explicit ThreadPool(size_t thread_count = 8): pool_ptr(std::make_shared<Pool>()) {
        assert(thread_count > 0);
        for(size_t i = 0; i < thread_count; i++) {
            /* 匿名函数方式创建，pool 为子线程内的智能指针，用于访问同一个任务队列 */
            std::thread([pool = pool_ptr] {
                /* 子线程对任务队列操作时需加锁 */
                std::unique_lock<std::mutex> locker(pool->mtx);
                while(true) {
                    if(!pool->tasks.empty()) {
                        auto task = std::move(pool->tasks.front());         // 转移任务对象
                        pool->tasks.pop();
                        locker.unlock();
                        task();                                             // 处理任务
                        locker.lock();
                    }
                    else if(pool->is_closed) break;                         // 线程池关闭，子线程停止运行
                    else pool->cond.wait(locker);
                }
            }).detach();
        }
    }

    /* 析构函数，线程池引用计数不为零时，通知所有子线程关闭 */
    ~ThreadPool() {
        if(static_cast<bool>(pool_ptr)) {
            {
                std::lock_guard<std::mutex> locker(pool_ptr->mtx);
                pool_ptr->is_closed = true;
            }
            pool_ptr->cond.notify_all();
        }
    }

    template<class F>
    /* 向任务队列中添加新的任务 */
    void add_task(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_ptr->mtx);
            pool_ptr->tasks.emplace(std::forward<F>(task));                 // 右值引用
        }
        pool_ptr->cond.notify_one();                                        // 通知一个子线程进行处理
    }

private:
    /* 线程池定义 */
    struct Pool {
        bool is_closed;                                                     // 是否关闭线程池
        std::mutex mtx;                                                     // 访问任务队列的互斥锁
        std::condition_variable cond;                                       // 任务队列的信号量
        std::queue<std::function<void()>> tasks;                            // 任务队列，对象为函数对象
    };
    std::shared_ptr<Pool> pool_ptr;
};

#endif
