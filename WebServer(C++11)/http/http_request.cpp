/*
 * @Description  : HTTP 请求
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#include "http_request.h"
using namespace std;

const char CRLF[] = "\r\n";

const unordered_set<string> HttpRequest::DEFAULT_HTML {
        "/index", "/sign", "/login",
        "/welcome", "/get_video", "/get_picture",
};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
        {"/sign.html", 0},
        {"/login.html", 1},
};

void HttpRequest::init() {
    method = path = version = body = "";
    state = REQUEST_LINE;
    linger = false;
    content_len = 0;
    header.clear();
    post.clear();
}

bool HttpRequest::is_keep_alive() const {
    if (header.count("Connection"))
        return linger;
    return false;
}

HTTP_CODE HttpRequest::parse(Buffer& buff) {
    while(buff.readable_bytes()) {
        const char* line_end;
        std::string line;
        // 除了消息体外，逐行解析
        if(state != BODY) {
            // search，找到返回第一个字符串下标，找不到返回最后一下标
            line_end = search(buff.read_ptr(), buff.write_ptr_const(), CRLF, CRLF + 2);
            // 如果没找到 CRLF，也不是 BODY，那么一定不完整
            if (line_end == buff.write_ptr())
                return NO_REQUEST;
            line = string(buff.read_ptr(), line_end);
            buff.retrieve_until(line_end + 2); // 除消息体外，都有换行符
        }
        else {
            // 消息体读取全部内容，同时清空缓存
            body += buff.retrieve_all_to_str();
            if (body.size() < content_len) {
                return NO_REQUEST;
            }
        }
        switch(state)
        {
            case REQUEST_LINE:
            {
                HTTP_CODE ret = parse_request_line(line);
                if(ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                parse_path();
                break;
            }
            case HEADERS:
            {
                HTTP_CODE ret = parse_header(line);
                if(ret == GET_REQUEST) {
                    return GET_REQUEST;
                }
                break;
            }
            case BODY:
            {
                HTTP_CODE ret = parse_body();
                if(ret == GET_REQUEST) {
                    return GET_REQUEST;
                }
                break;
            }
            default:
                break;
        }
    }
    LOG_DEBUG("state: %d", (int)state);
    LOG_DEBUG("content length: %d", content_len);
    LOG_DEBUG("[%s], [%s], [%s]", method.c_str(), path.c_str(), version.c_str());
    // 缓存读空了，但请求还不完整，继续读
    return NO_REQUEST;
}

/* 解析地址 */
void HttpRequest::parse_path() {
    if(path == "/") {
        path = "/index.html";
    } else if (DEFAULT_HTML.count(path)) {
        path += ".html";
    }
}

/* 解析请求行 */
HTTP_CODE HttpRequest::parse_request_line(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch sub_match;
    if(regex_match(line, sub_match, patten)) {
        method = sub_match[1];
        path = sub_match[2];
        version = sub_match[3];
        state = HEADERS;
        return NO_REQUEST;
    }
    LOG_ERROR("RequestLine Error");
    return BAD_REQUEST;
}

/* 解析请求头 */
HTTP_CODE HttpRequest::parse_header(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch sub_match;
    if(regex_match(line, sub_match, patten)) {
        header[sub_match[1]] = sub_match[2];
        if (sub_match[1] == "Connection")
            linger = (sub_match[2] == "keep-alive");
        if (sub_match[1] == "Content-Length") {
            content_len = stoi(sub_match[2]);
        }
        return NO_REQUEST;
    }
    else if(content_len) {
        state = BODY;
        return NO_REQUEST;
    }
    else {
        return GET_REQUEST;
    }
}

/* 解析请求消息体，根据消息类型解析内容 */
HTTP_CODE HttpRequest::parse_body() {
    if (method == "POST" && header["Content-Type"] == "application/x-www-form-urlencoded")
    {
        parse_from_url(); // 解析post请求数据
        if (DEFAULT_HTML_TAG.count(path))
        {
            // tag=1:login, tag=0:sign
            int tag = DEFAULT_HTML_TAG.find(path)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (user_verify(post["username"], post["password"], tag)) {
                LOG_INFO("Success!");
                path = "/welcome.html";
            }
            else if(tag == 1){
                LOG_INFO("Login failed!");
                path = "/login_error.html";
            }
            else {
                LOG_INFO("Sign failed!");
                path = "/sign_error.html";
            }
        }
    }
    LOG_DEBUG("Body:%s len:%d", body.c_str(), body.size());
    return GET_REQUEST;
}

int HttpRequest::convert_hex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

/* 解析 urlEncoded 类型数据 */
void HttpRequest::parse_from_url() {
    if(body.size() == 0) return;

    string key, value;
    int num = 0;
    int n = body.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body[i];
        switch (ch) {
            case '=':
                key = body.substr(j, i - j);
                j = i + 1;
                break;
            case '+':
                body[i] = ' ';
                break;
            case '%':
                num = convert_hex(body[i + 1]) * 16 + convert_hex(body[i + 2]);
                body[i + 2] = num % 10 + '0';
                body[i + 1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                value = body.substr(j, i - j);
                j = i + 1;
                post[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                break;
        }
    }
    assert(j <= i);
    if(!post.count(key) && j < i) {
        value = body.substr(j, i - j);
        post[key] = value;
    }
}

bool HttpRequest::user_verify(const string &name, const string &pwd, bool is_login) {
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::get_instance());
    assert(sql);

    bool flag = false;
    char order[256] = { 0 };
    MYSQL_RES *res = nullptr;

    if(!is_login) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, passwd FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) {
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        if(is_login) {
            if(pwd == password) {
                flag = true;
            }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        }
        else {
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    if(!is_login && flag) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, passwd) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) {
            LOG_DEBUG( "Insert error!");
            flag = false;
        }
        flag = true;
    }
    SqlConnPool::get_instance()->free_conn(sql);
    LOG_DEBUG( "User verify success!!");
    return flag;
}

std::string HttpRequest::get_path() const{
    return path;
}

std::string& HttpRequest::get_path(){
    return path;
}
std::string HttpRequest::get_method() const {
    return method;
}

std::string HttpRequest::get_version() const {
    return version;
}
