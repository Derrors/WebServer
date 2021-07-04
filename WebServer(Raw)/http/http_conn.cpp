//
// Created by Derrors on 2021/5/8.
//

#include <mysql/mysql.h>
#include <fstream>
#include "http_conn.h"

// 定义 http 响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this webserver.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this webserver.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

Locker m_lock;
map<string, string> users;

// 对文件描述符设置非阻塞
int set_nonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);

    return old_option;
}

// 在内核事件表中注册读事件，ET 模式，选择开启 EPOLLONESHOT
void add_fd(int epollfd, int fd, bool one_shot, int trig_mode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == trig_mode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

// 从内核时间表删除描述符
void remove_fd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 将事件重置为 EPOLLONESHOT
void mod_fd(int epollfd, int fd, int ev, int trig_mode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == trig_mode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int HttpConn::m_user_count = 0;
int HttpConn::m_epollfd = -1;

// 初始化连接,外部调用初始化套接字地址
void HttpConn::init(int sockfd, const sockaddr_in &addr, char *root, int trig_mode, int close_log, string user, string passwd, string db_name) {
    m_sockfd = sockfd;
    m_address = addr;

    add_fd(m_epollfd, sockfd, true, m_trig_mode);
    m_user_count++;

    // 当浏览器出现连接重置时，可能是网站根目录出错或 http 响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_trig_mode = trig_mode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, db_name.c_str());

    init();
}

// 初始化新接受的连接, check_state 默认为分析请求行状态
void HttpConn::init() {
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

// 关闭连接，关闭一个连接，客户总量 -1
void HttpConn::close_conn(bool real_close) {
    if (real_close && (m_sockfd != -1)) {
        printf("close %d\n", m_sockfd);
        remove_fd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void HttpConn::init_mysql_result(ConnectionPool *connPool) {
    // 先从连接池中取一个连接
    MYSQL *mysql = NULL;
    ConnectionRAII mysql_conn(&mysql, connPool);

    // 在 user 表中检索 username，passwd 数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));

    // 从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    // 返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    // 从结果集中获取下一行，将对应的用户名和密码，存入 map 中
    while(MYSQL_ROW row = mysql_fetch_row(result)) {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

void HttpConn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

// 解析 http 请求行，获得请求方法，目标 url 及 http 版本号
HttpConn::HTTP_CODE HttpConn::parse_request_line(char *text)
{
    // 在 HTTP 报文中，请求行用来说明请求类型, 要访问的资源以及所使用的 HTTP 版本，其中各个部分之间通过 \t 或空格分隔
    m_url = strpbrk(text, " \t");                   // 请求行中最先含有空格和 \t 任一字符的位置并返回
    if (!m_url) return BAD_REQUEST;                     // 如果没有空格或 \t，则报文格式有误
    *m_url++ = '\0';                                    // 将该位置改为 \0，用于将前面数据取出

    // 取出数据，并通过与 GET 和 POST 比较，以确定请求方式
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0) {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;

    // m_url 此时跳过了第一个空格或 \t 字符，但不知道之后是否还有，将 m_url 向后偏移，通过查找，继续跳过空格和 \t 字符，指向请求资源的第一个字符
    m_url += strspn(m_url, " \t");

    // 使用与判断请求方式的相同逻辑，判断 HTTP 版本号
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");

    // 仅支持 HTTP/1.1
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    // 对请求资源前 7 个字符进行判断，有些报文的请求资源中会带有 http://
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    // 同样增加 https 情况
    if (strncasecmp(m_url, "https://", 8) == 0) {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    // 一般的不会带有上述两种符号，直接是单独的 / 或 / 后面带访问资源
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;

    //当 url 为 / 时，显示欢迎界面
    if (strlen(m_url) == 1)
        strcat(m_url, "index.html");

    // 请求行处理完毕，将主状态机转移处理请求头
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

// 解析 http 请求头信息或空行
HttpConn::HTTP_CODE HttpConn::parse_headers(char *text) {
    // 判断是空行还是请求头
    if (text[0] == '\0') {
        // 判断是 GET 还是 POST 请求
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    // 解析请求头部连接字段
    else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");                        // 跳过空格和 \t 字符
        if (strcasecmp(text, "keep-alive") == 0) {
            // 如果是长连接，则将 linger 标志设置为true
            m_linger = true;
        }
    }
    // 解析请求头部内容长度字段
    else if (strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    // 解析请求头部 HOST 字段
    else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else {
        LOG_INFO("Unknown header: %s", text);
    }
    return NO_REQUEST;
}

// 判断 http 请求是否被完整读入
HttpConn::HTTP_CODE HttpConn::parse_content(char *text) {
    // 判断 buffer 中是否读取了消息体
    if (m_read_idx >= (m_content_length + m_checked_idx)) {
        text[m_content_length] = '\0';

        // POST 请求中最后为输入的用户名和密码
        m_string = text;

        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 从状态机，用于解析一行内容, 返回值为行的读取状态，有 LINE_OK, LINE_BAD, LINE_OPEN
HttpConn::LINE_STATUS HttpConn::parse_line() {
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
        // temp 为将要分析的字节
        temp = m_read_buf[m_checked_idx];

        // 如果当前是 \r 字符，则有可能会读取到完整行
        if (temp == '\r') {
            // 下一个字符达到了 buffer 结尾，则接收不完整，需要继续接收
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            // 下一个字符是 \n，将 \r\n 改为 \0\0
            else if (m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            // 如果都不符合，则返回语法错误
            return LINE_BAD;
        }
        // 如果当前字符是 \n，也有可能读取到完整行, 一般是上次读取到 \r 就到 buffer 末尾了，没有接收完整，再次接收时会出现这种情况
        else if (temp == '\n') {
            // 前一个字符是 \r，则接收完整
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    // 并没有找到 \r\n，需要继续接收
    return LINE_OPEN;
}

HttpConn::HTTP_CODE HttpConn::do_request() {
    // 将初始化的 m_real_file 赋值为网站根目录
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    // 找到 m_url 中 / 的位置
    const char *p = strrchr(m_url, '/');

    // 实现登录和注册校验
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {

        // 根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        // 将用户名和密码提取出来 user=123&password=123
        char name[100], password[100];
        int i;

        // 以 & 为分隔符，前面的为用户名
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        if (*(p + 1) == '3') {
            // 如果是注册，先检测数据库中是否有重名的, 没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            // 判断 map 中能否找到重复的用户名
            if (users.find(name) == users.end()) {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/login.html");
                else
                    strcpy(m_url, "/sign.html");
            }
            else
                strcpy(m_url, "/sign_error.html");
        }
            // 如果是登录，直接判断
            // 若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2') {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/login_error.html");
        }
    }

    // 如果请求资源为 /0，表示跳转注册界面
    if (*(p + 1) == '0') {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/sign.html");
        // 将网站目录和 /register.html 进行拼接，更新到 m_real_file 中
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // 如果请求资源为 /1 ，表示跳转登录界面
    else if (*(p + 1) == '1') {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/login.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // POST 请求，跳转到 picture.html，即图片请求页面
    else if (*(p + 1) == '5') {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/get_picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // POST 请求，跳转到 video.html，即视频请求页面
    else if (*(p + 1) == '6') {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/get_video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // 如果以上均不符合，即不是登录和注册，直接将 url 与网站目录拼接
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    // 通过 stat 获取请求资源文件信息，成功则将信息更新到 m_file_stat 结构体
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    // 判断文件的权限，是否可读，不可读则返回 FORBIDDEN_REQUEST 状态, S_IROTH 表示其他用户可读
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    // 判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误，S_ISDIR (st_mode) 判断是否为目录
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    // 以只读方式获取文件描述符，通过 mmap 将该文件映射到内存中
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    // 避免文件描述符的浪费和占用
    close(fd);

    // 表示请求文件存在，且可以访问
    return FILE_REQUEST;
}

// 循环读取客户数据，直到无数据可读或对方关闭连接
bool HttpConn::read_once() {
    if (m_read_idx >= READ_BUFFER_SIZE) return false;

    int bytes_read = 0;

    // LT 模式读取数据
    if (0 == m_trig_mode) {
        // 从套接字中读取数据，并记录读缓冲区中数据的长度
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if (bytes_read <= 0) return false;

        return true;
    }
    else {
        // 非阻塞 ET 工作模式下，需要一次性将数据读完
        while (true) {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);

            if (bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                return false;
            }
            else if (bytes_read == 0) {
                return false;
            }
            m_read_idx += bytes_read;
        }

        return true;
    }
}

// process_read 通过 while 循环，将主从状态机进行封装，对报文的每一行进行循环处理
HttpConn::HTTP_CODE HttpConn::process_read() {
    // 初始化从状态机状态、HTTP 请求解析结果
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    // parse_line 为从状态机的具体实现
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)) {
        // 从状态机已提前将一行的末尾字符 \r\n 变为 \0\0 ，所以 text 可以直接取出完整的行进行解析
        text = get_line();

        // m_start_line 是每一个数据行在 m_read_buf 中的起始位置, m_checked_idx 表示从状态机在 m_read_buf 中读取的位置
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);

        // 主状态机的三种状态转移逻辑
        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                // 解析请求行
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER: {
                // 解析请求头
                ret = parse_headers(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;

                // 完整解析 GET 请求后，跳转到报文响应函数
                else if (ret == GET_REQUEST) {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT: {
                // 解析消息体
                ret = parse_content(text);

                // 完整解析POST请求后，跳转到报文响应函数
                if (ret == GET_REQUEST)
                    return do_request();

                // 解析完消息体即完成报文解析，避免再次进入循环，更新 line_status
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

bool HttpConn::add_response(const char *format, ...) {
    // 如果写入内容超出 m_write_buf 大小则报错
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;

    va_list arg_list;                       // 定义可变参数列表
    va_start(arg_list, format);             // 将变量 arg_list 初始化为传入参数

    // 将数据 format 从可变参数列表写入缓冲区写，返回写入数据的长度
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

    // 如果写入的数据长度超过缓冲区剩余空间，则报错
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
        va_end(arg_list);
        return false;
    }

    // 更新 m_write_idx 位置
    m_write_idx += len;
    va_end(arg_list);                       // 清空可变参列表

    LOG_INFO("request:%s", m_write_buf);

    return true;
}

// 添加状态行
bool HttpConn::add_status_line(int status, const char *title) { return add_response("%s %d %s\r\n", "HTTP/1.1", status, title); }

// 添加消息报头，具体的添加文本长度、连接状态和空行
bool HttpConn::add_headers(int content_len) { return add_content_length(content_len) && add_linger() && add_blank_line(); }

// 添加 Content-Length，表示响应报文的长度
bool HttpConn::add_content_length(int content_len) { return add_response("Content-Length:%d\r\n", content_len); }

// 添加文本类型，这里是 html
bool HttpConn::add_content_type() { return add_response("Content-Type:%s\r\n", "text/html"); }

// 添加连接状态，通知浏览器端是保持连接还是关闭
bool HttpConn::add_linger() { return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close"); }

// 添加空行
bool HttpConn::add_blank_line() { return add_response("%s", "\r\n"); }

// 添加文本 content
bool HttpConn::add_content(const char *content) { return add_response("%s", content); }

bool HttpConn::process_write(HTTP_CODE ret) {
    switch (ret) {
        // 内部错误，500
        case INTERNAL_ERROR: {
            add_status_line(500, error_500_title);                  // 状态行
            add_headers(strlen(error_500_form));                           // 消息报头
            if (!add_content(error_500_form))
                return false;
            break;
        }
        // 报文语法有误，404
        case BAD_REQUEST: {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form))
                return false;
            break;
        }
        // 资源没有访问权限，403
        case FORBIDDEN_REQUEST: {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form))
                return false;
            break;
        }
        // 文件存在，200
        case FILE_REQUEST: {
            add_status_line(200, ok_200_title);
            //如果请求的资源存在
            if (m_file_stat.st_size != 0) {
                add_headers(m_file_stat.st_size);
                // 第一个 iovec 指针指向响应报文缓冲区，长度为 m_write_idx
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                // 第二个 iovec 指针指向 mmap 返回的文件指针，长度为文件大小
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;

                // 发送的全部数据为响应报文头部信息和文件大小
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else {
                // 如果请求的资源大小为 0，则返回空白 html 文件
                const char *ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if (!add_content(ok_string)) return false;
            }
        }
        default:
            return false;
    }
    // 除 FILE_REQUEST 状态外，其余状态只申请一个 iovec，指向响应报文缓冲区
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

bool HttpConn::write() {
    int temp = 0;

    // 若要发送的数据长度为 0, 表示响应报文为空，一般不会出现这种情况
    if (bytes_to_send == 0) {
        mod_fd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
        init();
        return true;
    }

    while (1) {
        // 将响应报文的状态行、消息头、空行和响应正文发送给浏览器端
        temp = writev(m_sockfd, m_iv, m_iv_count);

        // 正常发送时，temp 为发送的字节数
        if (temp < 0) {
            // 判断缓冲区是否满了
            if (errno == EAGAIN) {
                // 重新注册写事件
                mod_fd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_mode);
                return true;
            }
            // 如果发送失败，但不是缓冲区问题，取消映射
            unmap();
            return false;
        }

        bytes_have_send += temp;                // 更新已发送字节
        bytes_to_send -= temp;                  // 偏移文件 iovec 的指针

        // 第一个 iovec 头部信息的数据已发送完，发送第二个 iovec 数据
        if (bytes_have_send >= m_iv[0].iov_len) {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        // 继续发送第一个 iovec 头部信息的数据
        else {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        // 发送完毕时，重置连接
        if (bytes_to_send <= 0) {
            unmap();
            // 在 epoll 树上重置 EPOLLONESHOT 事件
            mod_fd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);

            // 浏览器的请求为长连接
            if (m_linger) {
                // 重新初始化 HTTP 对象
                init();
                return true;
            }
            else
                return false;
        }
    }
}

// 各子线程通过 process 函数对任务进行处理
void HttpConn::process() {
    HTTP_CODE read_ret = process_read();

    // NO_REQUEST，表示请求不完整，需要继续接收请求数据
    if (read_ret == NO_REQUEST) {
        // 重置并监听读事件
        mod_fd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
        return;
    }

    // 调用 process_write 完成报文响应
    bool write_ret = process_write(read_ret);
    if (!write_ret) close_conn();

    // 重置并监听写事件
    mod_fd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_mode);
}
