#ifndef CONFIG_H
#define CONFIG_H

#include "../webserver/webserver.h"

using namespace std;

class Config {
public:
    int PORT;                       // 端口号
    int LOG_WRITE;                  // 日志写入方式
    int TRIG_MODE;                  // 触发组合模式
    int LISTEN_TRIG_MODE;           // listenfd 触发模式
    int CONN_TRIG_MODE;             // connfd 触发模式
    int OPT_LINGER;                 // 优雅关闭链接
    int SQL_NUM;                    // 数据库连接池数量
    int THREAD_NUM;                 // 线程池内的线程数量
    int CLOSE_LOG;                  // 是否关闭日志
    int ACTOR_MODEL;                // 并发模型选择

    Config();
    ~Config() {};

    void parse_arg(int argc, char*argv[]);
};

#endif