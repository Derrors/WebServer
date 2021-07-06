/*
 * @Description  : HTTP 连接
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#include "http_connect.h"
using namespace std;

const char* HttpConn::src_dir;
std::atomic<int> HttpConn::user_count;
bool HttpConn::is_ET;

HttpConn::HttpConn() {
    fd = -1;
    addr = {0};
    is_close = true;
};

HttpConn::~HttpConn() {
    Close();
};

/* 连接初始化：套接字，端口，缓存，请求解析状态机 */
void HttpConn::init(int sock_fd, const sockaddr_in& addr_) {
    assert(sock_fd > 0);
    user_count++;
    addr = addr_;
    fd = sock_fd;
    buffer_write.retrieve_all();
    buffer_read.retrieve_all();
    is_close = false;
    LOG_INFO("Client[%d](%s:%d) in, user_count:%d", fd, get_ip(), get_port(), (int)user_count);
    request.init();                 // 在连接时初始化，而不是请求到来时，避免一次请求分多次发送，状态机状态重置
}

/* 关闭连接 */
void HttpConn::Close() {
    response.unmap_file();
    if(!is_close){
        is_close = true;
        user_count--;
        close(fd);
        LOG_INFO("Client[%d](%s:%d) quit, user_count:%d", fd, get_ip(), get_port(), (int)user_count);
    }
}

int HttpConn::get_fd() const {
    return fd;
};

struct sockaddr_in HttpConn::get_addr() const {
    return addr;
}

const char* HttpConn::get_ip() const {
    return inet_ntoa(addr.sin_addr);
}

int HttpConn::get_port() const {
    return addr.sin_port;
}

/* 读方法，ET模式会将缓存读空 */
// 返回最后一次读取的长度，以及错误类型
ssize_t HttpConn::read(int* save_errno) {
    ssize_t len = -1;
    do {
        len = buffer_read.read_fd(fd, save_errno);
        if (len <= 0) {
            *save_errno = errno;
            break;
        }
    } while (is_ET);
    return len;
}

/* 写方法,响应头和响应体分开传输 */
ssize_t HttpConn::write(int* save_errno) {
    ssize_t len = -1;
    do {
        len = writev(fd, iov, iov_count);
        if(len <= 0) {
            *save_errno = errno;
            break;
        }
        if(iov[0].iov_len + iov[1].iov_len  == 0) {
            break;                                                              // 传输结束
        }
        else if(static_cast<size_t>(len) > iov[0].iov_len) {
            iov[1].iov_base = (uint8_t*) iov[1].iov_base + (len - iov[0].iov_len);
            iov[1].iov_len -= (len - iov[0].iov_len);
            if(iov[0].iov_len) {
                buffer_write.retrieve_all();
                iov[0].iov_len = 0;
            }
        }
        else {
            iov[0].iov_base = (uint8_t*)iov[0].iov_base + len;
            iov[0].iov_len -= len;
            buffer_write.retrieve(len);
        }
    } while(is_ET || to_write_bytes() > 10240);
    return len;
}

/* 处理方法：解析读缓存内的请求报文，判断是否完整 */
// 不完整返回 false，完整在写缓存内写入响应头，并获取响应体内容（文件）
bool HttpConn::process() {
    // request.init();
    if(buffer_read.readable_bytes() <= 0) {
        return false;
    }
    else if(request.parse(buffer_read)) {
        LOG_DEBUG("%s", request.get_path().c_str());
        response.init(src_dir, request.get_path(), request.is_keep_alive(), 200);
        request.init(); // 如果是长连接，等待下一次请求，需要初始化
    } else {
        response.init(src_dir, request.get_path(), false, 400);
    }

    response.make_response(buffer_write);
    /* 响应头 */
    iov[0].iov_base = const_cast<char*>(buffer_write.read_ptr());
    iov[0].iov_len = buffer_write.readable_bytes();
    iov_count = 1;

    /* 文件 */
    if(response.file_len() > 0  && response.file()) {
        iov[1].iov_base = response.file();
        iov[1].iov_len = response.file_len();
        iov_count = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response.file_len() , iov_count, to_write_bytes());
    return true;
}