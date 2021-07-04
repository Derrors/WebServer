#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

ConnectionPool::ConnectionPool() {
	m_cur_conn = 0;
	m_free_conn = 0;
}

ConnectionPool *ConnectionPool::get_instance() {
	static ConnectionPool connPool;
	return &connPool;
}

void ConnectionPool::init(string url, string user, string passwd, string db_name, int port, int max_conn, int close_log) {
    // 初始化数据库信息
	m_url = url;
	m_user = user;
	m_passwd = passwd;
    m_database_name = db_name;
    m_port = port;
	m_close_log = close_log;

    // 创建 max_conn 条数据库连接
	for (int i = 0; i < max_conn; i++) {
		MYSQL *conn = NULL;
		conn = mysql_init(conn);

		if (conn == NULL) {
			LOG_ERROR("MySQL Error");
			exit(1);
		}

		conn = mysql_real_connect(conn, url.c_str(), user.c_str(), passwd.c_str(), db_name.c_str(), port, NULL, 0);

		if (conn == NULL) {
			LOG_ERROR("MySQL Error");
			exit(1);
		}

        // 更新连接池和空闲连接数量
        conn_list.push_back(conn);
		++m_free_conn;
	}

    // 将信号量初始化为最大连接次数
	reserve = Sem(m_free_conn);
	m_max_conn = m_free_conn;
}

// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *ConnectionPool::get_connection() {
	MYSQL *conn = NULL;

	if (0 == conn_list.size()) return NULL;

	reserve.wait();                         // 取出连接，信号量原子减1，为0则等待
	lock.lock();

	conn = conn_list.front();
    conn_list.pop_front();

	--m_free_conn;
	++m_cur_conn;

	lock.unlock();
	return conn;
}

// 释放当前使用的连接
bool ConnectionPool::release_connection(MYSQL *conn) {
	if (NULL == conn) return false;

	lock.lock();

	conn_list.push_back(conn);
	++m_free_conn;
	--m_cur_conn;

	lock.unlock();

	reserve.post();
	return true;
}

// 销毁数据库连接池
void ConnectionPool::destroy_pool() {
	lock.lock();
	if (conn_list.size() > 0) {
		list<MYSQL *>::iterator it;
		for (it = conn_list.begin(); it != conn_list.end(); ++it) {
			MYSQL *conn = *it;
			mysql_close(conn);
		}
		m_cur_conn = 0;
		m_free_conn = 0;
		conn_list.clear();
	}

	lock.unlock();
}

int ConnectionPool::get_free_conn() { return this->m_free_conn; }

ConnectionPool::~ConnectionPool() { destroy_pool(); }

ConnectionRAII::ConnectionRAII(MYSQL **SQL, ConnectionPool *connPool) {
	*SQL = connPool->get_connection();
	
	conRAII = *SQL;
	poolRAII = connPool;
}

ConnectionRAII::~ConnectionRAII(){ poolRAII->release_connection(conRAII); }