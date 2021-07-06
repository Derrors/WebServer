/*
 * @Description  : Buffer 定义
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
#include <atomic>
#include <assert.h>

class Buffer {
public:
    Buffer(int init_size=1024);
    ~Buffer() = default;

    size_t writeable_bytes() const;             // 获取缓冲区当前可写的字节数，即还剩多少空
    size_t readable_bytes() const;              // 获取缓冲区当前可读的字节数，即已经写了多少
    size_t front_bytes() const;                 // 获取缓冲区前部可使用空间的大小


    void ensure_writeable(size_t len);

    void retrieve(size_t len);
    void retrieve_until(const char* end);
    void retrieve_all();
    std::string retrieve_all_to_str();

    const char* read_ptr() const;
    const char* write_ptr_const() const;
    char* write_ptr();

    void append(const char* str, size_t len);
    void append(const void* data, size_t len);
    void append(const std::string& str);
    void append(const Buffer& buff);

    ssize_t read_fd(int fd, int* Errno);
    ssize_t write_fd(int fd, int* Errno);
    void move_write_ptr(size_t len);

private:
    char* begin_ptr();
    const char* begin_ptr() const;
    void adjust_space(size_t len);

    std::vector<char> buffer;                   // 缓冲区容器
    std::atomic<std::size_t> read_pos;          // 缓冲区读取位置索引，原子对象
    std::atomic<std::size_t> write_pos;         // 缓冲区写入位置索引，原子对象
};

#endif
