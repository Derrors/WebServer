#include "config/config.h"

int main(int argc, char *argv[]) {
    // 配置数据库信息,登录名,密码,库名
    string user = "root";
    string passwd = "lqh19961001";
    string db_name = "web_server_db";

    // 命令行参数解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    // 服务端初始化
    server.init(config.PORT, user, passwd, db_name, config.LOG_WRITE,
                config.OPT_LINGER, config.TRIG_MODE,  config.SQL_NUM,  config.THREAD_NUM,
                config.CLOSE_LOG, config.ACTOR_MODEL);

    server.log_write();                         // 日志
    server.sql_pool();                          // 数据库
    server.thread_pool();                       // 线程池
    server.trig_mode();                         // 触发模式
    server.event_listen();                      // 监听
    server.event_loop();                        // 运行

    return 0;
}