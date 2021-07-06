/*
 * @Description  : HTTP 请求
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H


#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>
#include <mysql/mysql.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sql_connection_pool.h"

enum PARSE_STATE {
    REQUEST_LINE,
    HEADERS,
    BODY,
    FINISH,
};

enum HTTP_CODE {
    NO_REQUEST = 0,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURSE,
    FORBIDDENT_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION,
};

class HttpRequest {
public:

    HttpRequest() { init(); }
    ~HttpRequest() = default;

    void init();
    HTTP_CODE parse(Buffer& buff);

    std::string get_path() const;
    std::string& get_path();
    std::string get_method() const;
    std::string get_version() const;

    bool is_keep_alive() const;

private:
    HTTP_CODE parse_request_line(const std::string& line);
    HTTP_CODE parse_header(const std::string& line);
    HTTP_CODE parse_body();
    void parse_path();
    void parse_from_url();

    static bool user_verify(const std::string& name, const std::string& pwd, bool is_login);

    PARSE_STATE state;
    std::string method, path, version, body;
    bool linger;
    size_t content_len;
    std::unordered_map<std::string, std::string> header;
    std::unordered_map<std::string, std::string> post;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int convert_hex(char ch);
};


#endif
