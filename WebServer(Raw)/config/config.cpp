#include "config.h"

Config::Config() {
    PORT = 9006;                                // 端口号,默认 9006
    LOG_WRITE = 0;                              // 日志写入方式，默认同步
    TRIG_MODE = 0;                              // 触发组合模式,默认 listenfd LT + connfd LT=
    LISTEN_TRIG_MODE = 0;                       // listenfd 触发模式，默认 LT
    CONN_TRIG_MODE = 0;                         // connfd 触发模式，默认LT
    OPT_LINGER = 0;                             // 优雅关闭链接，默认不使用
    SQL_NUM = 8;                                // 数据库连接池数量,默认8
    THREAD_NUM = 8;                             // 线程池内的线程数量,默认8
    CLOSE_LOG = 0;                              // 关闭日志,默认不关闭
    ACTOR_MODEL = 0;                            // 并发模型,默认是 proactor
}

void Config::parse_arg(int argc, char*argv[]) {
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt) {
            case 'p':
                PORT = atoi(optarg);
                break;
            case 'l':
                LOG_WRITE = atoi(optarg);
                break;
            case 'm':
                TRIG_MODE = atoi(optarg);
                break;
            case 'o':
                OPT_LINGER = atoi(optarg);
                break;
            case 's':
                SQL_NUM = atoi(optarg);
                break;
            case 't':
                THREAD_NUM = atoi(optarg);
                break;
            case 'c':
                CLOSE_LOG = atoi(optarg);
                break;
            case 'a':
                ACTOR_MODEL = atoi(optarg);
                break;
            default:
                break;
        }
    }
}