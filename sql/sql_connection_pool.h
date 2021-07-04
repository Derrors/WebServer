#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class ConnectionPool {
private:
	int m_max_conn;                                     // 最大连接数
	int m_cur_conn;                                     // 当前已使用的连接数
	int m_free_conn;                                    // 当前空闲的连接数
	Locker lock;
	list<MYSQL *> conn_list;                            // 连接池
	Sem reserve;

    ConnectionPool();
    ~ConnectionPool();

public:
	string m_url;			                            // 主机地址
	string m_port;		                                // 数据库端口号
	string m_user;		                                // 登陆数据库用户名
	string m_passwd;	                                // 登陆数据库密码
	string m_database_name;                             // 数据库名
	int m_close_log;	                                // 日志开关

    // 单例模式
    static ConnectionPool *get_instance();
    void init(string url, string user, string passwd, string db_name, int port, int max_conn, int close_log);

    MYSQL *get_connection();				            // 获取一个可用的数据库连接
    bool release_connection(MYSQL *conn);               // 释放当前连接
    int get_free_conn();					            // 获取可用连接数
    void destroy_pool();	                            // 销毁所有连接
};

// RAII 机制释放数据库连接
class ConnectionRAII{
private:
    MYSQL *conRAII;
    ConnectionPool *poolRAII;

public:
    ConnectionRAII(MYSQL **con, ConnectionPool *connPool);
	~ConnectionRAII();
};

#endif
