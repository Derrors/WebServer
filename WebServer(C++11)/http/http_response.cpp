/*
 * @Description  : HTTP 响应
 * @Author       : Qinghe Li
 * @Create time  : 2021-07-04 20:19:35
 * @Last update  : 2021-07-04 20:21:53
 */

#include "http_response.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
        { ".html",  "text/html" },
        { ".xml",   "text/xml" },
        { ".xhtml", "application/xhtml+xml" },
        { ".txt",   "text/plain" },
        { ".rtf",   "application/rtf" },
        { ".pdf",   "application/pdf" },
        { ".word",  "application/msword" },
        { ".png",   "image/png" },
        { ".gif",   "image/gif" },
        { ".jpg",   "image/jpeg" },
        { ".jpeg",  "image/jpeg" },
        { ".au",    "audio/basic" },
        { ".mpeg",  "video/mpeg" },
        { ".mpg",   "video/mpeg" },
        { ".avi",   "video/x-msvideo" },
        { ".gz",    "application/x-gzip" },
        { ".tar",   "application/x-tar" },
        { ".css",   "text/css "},
        { ".js",    "text/javascript "},
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
        { 200, "OK" },
        { 400, "Bad Request" },
        { 403, "Forbidden" },
        { 404, "Not Found" },
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
        { 400, "/index.html" },
        { 403, "/index.html" },
        { 404, "/index.html" },
};

HttpResponse::HttpResponse() {
    code = -1;
    path = src_dir = "";
    is_keep_alive = false;
    mm_file = nullptr;
    mm_file_stat = { 0 };
};

HttpResponse::~HttpResponse() {
    unmap_file();
}

void HttpResponse::init(const string& srcDir, string& path_, bool is_keep_alive_, int code_){
    assert(srcDir != "");
    if(mm_file) { unmap_file(); }
    code = code_;
    is_keep_alive = is_keep_alive_;
    path = path_;
    src_dir = srcDir;
    mm_file = nullptr;
    mm_file_stat = { 0 };
}

void HttpResponse::make_response(Buffer &buff) {
    /* 判断请求的资源文件 */
    if(stat((src_dir + path).data(), &mm_file_stat) < 0 || S_ISDIR(mm_file_stat.st_mode)) {
        code = 404;
    }
    else if(!(mm_file_stat.st_mode & S_IROTH)) {
        code = 403;
    }
    else if(code == -1) {
        code = 200;
    }
    error_html();
    add_state_line(buff);
    add_header(buff);
    add_content(buff);
}

char* HttpResponse::file() {
    return mm_file;
}

size_t HttpResponse::file_len() const {
    return mm_file_stat.st_size;
}

void HttpResponse::error_html() {
    if(CODE_PATH.count(code) == 1) {
        path = CODE_PATH.find(code)->second;
        stat((src_dir + path).data(), &mm_file_stat);
    }
}

void HttpResponse::add_state_line(Buffer &buff) {
    string status;
    if(CODE_STATUS.count(code) == 1) {
        status = CODE_STATUS.find(code)->second;
    }
    else {
        code = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.append("HTTP/1.1 " + to_string(code) + " " + status + "\r\n");
}

void HttpResponse::add_header(Buffer &buff) {
    buff.append("Connection: ");
    if(is_keep_alive) {
        buff.append("keep-alive\r\n");
        buff.append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buff.append("close\r\n");
    }
    buff.append("Content-type: " + get_file_type() + "\r\n");
}

void HttpResponse::add_content(Buffer& buff) {
    int src_fd = open((src_dir + path).data(), O_RDONLY);
    if(src_fd < 0) {
        error_content(buff, "File Not Found!");
        return;
    }

    /* 将文件映射到内存提高文件的访问速度 MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    LOG_DEBUG("file path %s", (src_dir + path).data());
    int* mm_ret = (int*)mmap(0, mm_file_stat.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
    if(*mm_ret == -1) {
        error_content(buff, "File Not Found!");
        return;
    }
    mm_file = (char*)mm_ret;
    close(src_fd);
    buff.append("Content-length: " + to_string(mm_file_stat.st_size) + "\r\n\r\n");
}

void HttpResponse::unmap_file() {
    if(mm_file) {
        munmap(mm_file, mm_file_stat.st_size);
        mm_file = nullptr;
    }
}

string HttpResponse::get_file_type() {
    /* 判断文件类型 */
    string::size_type idx = path.find_last_of('.');
    if(idx == string::npos) {
        return "text/plain";
    }
    string suffix = path.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

/* 范围外错误页面 */
void HttpResponse::error_content(Buffer &buff, std::string message) {
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code) == 1) {
        status = CODE_STATUS.find(code)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(code) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em> Derrors's WebServer </em></body></html>";

    buff.append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.append(body);
}