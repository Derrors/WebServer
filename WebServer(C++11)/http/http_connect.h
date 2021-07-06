/*
 * @Description  : HTTP 连接
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#ifndef HTTP_CONNECT_H
#define HTTP_CONNECT_H


#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>

#include "../log/log.h"
#include "../pool/sql_connection_pool.h"
#include "../buffer/buffer.h"
#include "http_request.h"
#include "http_response.h"

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    void init(int sock_fd, const sockaddr_in& addr);

    ssize_t read(int* save_errno);
    ssize_t write(int* save_errno);

    int get_fd() const;
    int get_port() const;
    const char* get_ip() const;
    sockaddr_in get_addr() const;

    bool process();
    void Close();

    int to_write_bytes() {
        return iov[0].iov_len + iov[1].iov_len;
    }

    bool is_keep_alive() const {
        return request.is_keep_alive();
    }

    static bool is_ET;
    static const char* src_dir;
    static std::atomic<int> user_count;

private:
    int fd;
    struct  sockaddr_in addr;

    int iov_count;
    struct iovec iov[2];

    Buffer buffer_read;                                 // 读缓冲区
    Buffer buffer_write;                                // 写缓冲区

    HttpRequest request;
    HttpResponse response;

    bool is_close;
};


#endif
