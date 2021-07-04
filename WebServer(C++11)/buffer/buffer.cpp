/*
 * @Description  :
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#include "buffer.h"

Buffer::Buffer(int init_size) : buffer(init_size), read_pos(0), write_pos(0) {}

/* 返回缓冲区可读的字节数 */
size_t Buffer::readable_bytes() const {
    return write_pos - read_pos;
}
/* 返回缓冲区可写的字节数 */
size_t Buffer::writeable_bytes() const {
    return buffer.size() - write_pos;
}
/* 返回缓冲区头部可利用的字节数 */
size_t Buffer::front_bytes() const {
    return read_pos;
}
/* 返回指向第一个可读字符的指针 */
const char* Buffer::peek() {
    return begin_ptr() + read_pos;
}
/* 从 read_pos 开始取出长度为 len 的数据 */
void Buffer::retrieve(size_t len) {
    assert(len <= readable_bytes());
    read_pos += len;
}
/* 取出 end 之前的数据 */
void Buffer::retrieve_until(const char *end) {
    assert(peek() <= end);
    retrieve(end - peek());
}
/* 取出缓冲区中的全部数据 */
void Buffer::retrieve_all() {
    // 清空缓冲区
    bzero(&buffer[0], buffer.size());
    read_pos = 0;
    write_pos = 0;
}
/* 取得缓冲区中的所有数据并返回 */
std::string Buffer::retrieve_all_to_str() {
    std::string str(peek(), readable_bytes());
    retrieve_all();
    return str;
}
/* 返回指向写入数据位置的指针 */
const char* Buffer::begin_write() const {
    return begin_ptr() + write_pos;
}

char* Buffer::begin_write() {
    return begin_ptr() + write_pos;
}

/* 在缓冲区中写入数据 */
void Buffer::append(const char* str, size_t len) {
    assert(str);
    ensure_writeable(len);
    std::copy(str, str + len, begin_write());
    write_pos += len;
}
void Buffer::append(const void* data, size_t len) {
    assert(data);
    append(static_cast<const char*>(data), len);
}

void Buffer::append(const std::string& str) {
    append(str.data(), str.length());
}

void Buffer::append(const Buffer& buff) {
    append(buff.peek(), buff.readable_bytes());
}

/* 确保缓冲区可写入长度 len 的数据，若空间不够，则自动增大缓冲区 */
void Buffer::ensure_writeable(size_t len) {
    if(writeable_bytes() < len) {
        adjust_space(len);
    }
    assert(writeable_bytes() >= len);
}

/* 从文件描述符读取数据到缓冲区，返回读取的数据长度 */
ssize_t Buffer::read_fd(int fd, int* save_errno) {
    char buff[65535];
    struct iovec iov[2];
    const size_t writeable = writeable_bytes();
    /* 分散读，保证数据全部读完 */
    iov[0].iov_base = begin_ptr() + write_pos;
    iov[0].iov_len = writeable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *save_errno = errno;
    }
    else if(static_cast<size_t>(len) <= writeable) {
        write_pos += len;
    }
    else {
        write_pos = buffer.size();
        append(buff, len - writeable);
    }
    return len;
}

/* 将缓冲区的数据写入文件描述符，返回写入长度 */
ssize_t Buffer::write_fd(int fd, int *save_errno) {
    size_t read_size = readable_bytes();
    ssize_t len = write(fd, peek(), read_size);
    if(len < 0) {
        *save_errno = errno;
        return len;
    }
    read_pos += len;
    return len;
}
/* 返回指向缓冲区开始位置的指针 */
char* Buffer::begin_ptr() {
    return &*buffer.begin();
}

const char* Buffer::begin_ptr() const {
    return &*buffer.begin();
}

/* 根据缓冲区中可用空间大小来调整或扩大缓冲区 */
void Buffer::adjust_space(size_t len) {
    if(writeable_bytes() + front_bytes() < len) {
        buffer.resize(write_ptr + len + 1);
    }
    else {
        size_t readable = readable_bytes();
        // 对缓冲区中的数据进行紧凑到头部位置
        std::copy(begin_ptr() + read_pos, begin_ptr() + write_pos, begin_ptr());
        read_pos = 0;
        write_pos = read_pos + readable;
        assert(readable == readable_bytes());
    }
}
