//
// Created by Derrors on 2021/7/5.
//

#include "log.h"
using namespace std;

Log::Log() {
    line_count = 0;
    is_asyc = false;
    write_thread = nullptr;
    deque = nullptr;
    today = 0;
    fp = nullptr;
}

Log::~Log() {
    if(write_thread && write_thread->joinable()) {
        while(!deque->empty()) {
            deque->flush();
        };
        deque->Close();
        write_thread->join();
    }
    if(fp) {
        lock_guard<mutex> locker(mtx);
        flush();
        fclose(fp);
    }
}

int Log::get_level() {
    lock_guard<mutex> locker(mtx);
    return level;
}

void Log::set_level(int level_) {
    lock_guard<mutex> locker(mtx);
    level = level_;
}

/* 初始化日志对象 */
void Log::init(int level_ = 1, const char* path_, const char* suffix_, int max_que_size) {
    is_open_ = true;
    level = level_;
    if(max_que_size > 0) {
        is_asyc = true;
        if(!deque) {
            unique_ptr<BlockDeque<std::string>> new_deque(new BlockDeque<std::string>);
            deque = move(new_deque);

            std::unique_ptr<std::thread> new_thread(new thread(flush_log_thread));
            write_thread = move(new_thread);
        }
    } 
    else {
        is_asyc = false;
    }
    
    line_count = 0;
    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    path = path_;
    suffix = suffix_;
    char file_name[LOG_NAME_LEN] = {0};
    snprintf(file_name, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", path, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix);
    today = t.tm_mday;

    /* 初始化日志缓冲区，创建日志文件 */
    {
        lock_guard<mutex> locker(mtx);
        buff.retrieve_all();
        if(fp) {
            flush();
            fclose(fp);
        }

        fp = fopen(file_name, "a");
        if(fp == nullptr) {
            mkdir(path, 0777);
            fp = fopen(file_name, "a");
        }
        assert(fp != nullptr);
    }
}

/* 写日志 */
void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sys_time = localtime(&tSec);
    struct tm t = *sys_time;
    va_list vaList;

    /* 日志日期 日志行数 */
    if (today != t.tm_mday || (line_count && (line_count  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx);
        locker.unlock();

        char new_file[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (today != t.tm_mday)
        {
            snprintf(new_file, LOG_NAME_LEN - 72, "%s/%s%s", path, tail, suffix);
            today = t.tm_mday;
            line_count = 0;
        }
        else {
            snprintf(new_file, LOG_NAME_LEN - 72, "%s/%s-%d%s", path, tail, (line_count  / MAX_LINES), suffix);
        }

        locker.lock();
        flush();
        fclose(fp);
        fp = fopen(new_file, "a");
        assert(fp != nullptr);
    }
    /* 将 Log 缓冲区中的内容写入日志缓冲区 */
    {
        unique_lock<mutex> locker(mtx);
        line_count++;
        // 写时间
        int n = snprintf(buff.write_ptr(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                         t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);

        buff.move_write_ptr(n);
        // 写 LOG level
        add_log_title(level);

        // 格式化内容并写入缓冲区
        va_start(vaList, format);
        int m = vsnprintf(buff.write_ptr(), buff.writeable_bytes(), format, vaList);
        va_end(vaList);

        buff.move_write_ptr(m);
        buff.append("\n\0", 2);

        if(is_asyc && deque && !deque->full()) {
            deque->push_back(buff.retrieve_all_to_str());
        }
        else {
            fputs(buff.read_ptr(), fp);
        }
        // 写完清空缓冲区
        buff.retrieve_all();
    }
}

/* 增加日志类别标题 */
void Log::add_log_title(int level) {
    switch(level) {
        case 0:
            buff.append("[DEBUG]: ", 9);
            break;
        case 1:
            buff.append("[INFO] : ", 9);
            break;
        case 2:
            buff.append("[WARN] : ", 9);
            break;
        case 3:
            buff.append("[ERROR]: ", 9);
            break;
        default:
            buff.append("[INFO] : ", 9);
            break;
    }
}

/* 通知写线程来写日志 */
void Log::flush() {
    if(is_asyc) {
        deque->flush();
    }
    fflush(fp);
}

/* 异步写文件，写线程执行的函数 */
void Log::asyc_write() {
    string str = "";
    while(deque->pop(str)) {
        lock_guard<mutex> locker(mtx);
        fputs(str.c_str(), fp);
    }
}

Log* Log::get_instance() {
    static Log inst;
    return &inst;
}

void Log::flush_log_thread() {
    Log::get_instance()->asyc_write();
}