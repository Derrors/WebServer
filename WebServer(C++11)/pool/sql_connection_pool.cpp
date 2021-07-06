/*
 * @Description  : 数据库连接池
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#include "sql_connection_pool.h"
using namespace std;

SqlConnPool::SqlConnPool() {
    used_count = 0;
    free_count = 0;
}

/* 获取数据库连接池的唯一实例 */
SqlConnPool* SqlConnPool::get_instance() {
    static SqlConnPool conn_pool;
    return &conn_pool;
}

/* 初始化数据库连接池 */
void SqlConnPool::init(const char* host, int port, const char* user,const char* pwd, const char* db_name, int conn_size = 10) {
    assert(conn_size > 0);
    /* 预先创建一定数量的数据库连接，加入到连接队列中 */
    for (int i = 0; i < conn_size; i++) {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, user, pwd, db_name, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        conn_que.push(sql);
    }
    MAX_CONN = conn_size;
    sem_init(&sem, 0, MAX_CONN);
}

/* 在连接队列中取得一个数据库连接 */
MYSQL* SqlConnPool::get_conn() {
    MYSQL* sql = nullptr;
    if(conn_que.empty()){
        LOG_WARN("SqlConnPool is busy and there is no usable connection !");
        return nullptr;
    }
    sem_wait(&sem);
    {
        lock_guard<mutex> locker(mtx);
        sql = conn_que.front();
        conn_que.pop();
    }
    return sql;
}

/* 释放一个数据库连接，将其加入到连接队列中 */
void SqlConnPool::free_conn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(mtx);
    conn_que.push(sql);
    sem_post(&sem);
}

/* 关闭数据库连接池 */
void SqlConnPool::close_pool() {
    lock_guard<mutex> locker(mtx);
    while(!conn_que.empty()) {
        auto sql = conn_que.front();
        conn_que.pop();
        mysql_close(sql);
    }
    mysql_library_end();
}

/* 返回剩余可用的连接数量 */
int SqlConnPool::get_free_conn_count() {
    lock_guard<mutex> locker(mtx);
    return conn_que.size();
}

SqlConnPool::~SqlConnPool() {
    close_pool();
}