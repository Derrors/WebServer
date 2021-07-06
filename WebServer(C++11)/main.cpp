/*
 * @Description  : main
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#include "server/webserver.h"

int TRIG_MODE = 3;                      // 触发组合模式
int TIME_OUT = 60000;                   // 超时时间（毫秒）
bool OPT_LINGER = false;                // 优雅关闭链接
int THREAD_NUM = 8;                     // 线程池内的线程数量
int SQL_NUM = 12;                       // 数据库连接池数量

bool OPEN_LOG = false;                   // 是否开启日志
int LOG_LEVEL = 1;                      // 日志级别
int LOG_QUE_SIZE = 1024;                // 日志队列大小

int SERVER_PORT = 9006;                 // 端口号
int SQL_PORT = 3306;                    // 数据库端口

const char* SQL_USER = "root";                  // 数据库登录用户
const char* SQL_PWD = "lqh19961001";            // 数据库登录密码
const char* SQL_NAME = "web_server_db";         // 数据库名


int main(int argc, char *argv[]) {
    /*
    响应模式
        0：连接和监听都是LT
        1：连接ET，监听LT
        2：连接LT，监听ET
        3：连接和监听都是ET
    日志等级
        0：DEBUG
        1：INFO
        2：WARN
        3：ERROR
    */

    WebServer server(
        SERVER_PORT, TRIG_MODE, TIME_OUT, OPT_LINGER,
        SQL_PORT, SQL_USER, SQL_PWD, SQL_NAME, SQL_NUM,
        THREAD_NUM, OPEN_LOG, LOG_LEVEL, LOG_QUE_SIZE);

    server.start();

    return 0;
}