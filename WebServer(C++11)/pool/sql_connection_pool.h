/*
 * @Description  : 数据库连接池
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool* get_instance();

    MYSQL* get_conn();
    void free_conn(MYSQL* conn);
    int get_free_conn_count();

    void init(const char* host, int port,
              const char* user,const char* pwd,
              const char* db_name, int conn_size);
    void close_pool();

private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN;
    int used_count;
    int free_count;

    std::queue<MYSQL*> conn_que;
    std::mutex mtx;
    sem_t sem;
};

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool *conn_pool) {
        assert(conn_pool);
        *sql = conn_pool->get_conn();
        sql_RAII = *sql;
        conn_pool_RAII = conn_pool;
    }

    ~SqlConnRAII() {
        if(sql_RAII) {
            conn_pool_RAII->free_conn(sql_RAII);
        }
    }

private:
    MYSQL* sql_RAII;
    SqlConnPool* conn_pool_RAII;
};

#endif
