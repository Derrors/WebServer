/*
 * @Description  : HTTP 响应
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H


#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void init(const std::string& src_dir, std::string& path, bool is_keep_alive = false, int code = -1);
    void make_response(Buffer& buff);
    void unmap_file();
    char* file();
    size_t file_len() const;
    void error_content(Buffer& buff, std::string message);
    int get_code() const { return code; }

private:
    void add_state_line(Buffer &buff);
    void add_header(Buffer &buff);
    void add_content(Buffer &buff);

    void error_html();
    std::string get_file_type();

    int code;
    bool is_keep_alive;

    std::string path;
    std::string src_dir;

    char* mm_file;
    struct stat mm_file_stat;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
};


#endif
