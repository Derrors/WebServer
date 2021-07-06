//
// Created by Derrors on 2021/7/5.
//

#ifndef LOG_H
#define LOG_H


#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/stat.h>
#include "block_deque.h"
#include "../buffer/buffer.h"

class Log {
public:
    void init(int level, const char* path = "./log", const char* suffix =".log", int max_capacity = 1024);

    static Log* get_instance();
    static void flush_log_thread();

    void write(int level, const char *format,...);
    void flush();

    int get_level();
    void set_level(int level);
    bool is_open() { return is_open_; }

private:
    Log();
    void add_log_title(int level);
    virtual ~Log();
    void asyc_write();

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    const char* path;
    const char* suffix;
    Buffer buff;

    int max_lines;
    int line_count;
    int today;

    int level;
    bool is_asyc;
    bool is_open_;

    FILE* fp;
    std::unique_ptr<BlockDeque<std::string>> deque;
    std::unique_ptr<std::thread> write_thread;
    std::mutex mtx;
};

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::get_instance();\
        if (log->is_open() && log->get_level() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);


#endif
