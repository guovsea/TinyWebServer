#ifndef HTTPCONN_H
#define HTTPCONN_H
#include "../CGImysql/ConnPool.h"
#include "../sync.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
class HttpConn {
  public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0, // 解析请求行
        CHECK_STATE_HEADER,          // 解析请求头
        CHECK_STATE_CONTENT // 解析消息体，仅用于解析POST请求
    };
    enum HTTP_CODE {
        NO_REQUEST = 0, // 请求不完整，需要继续读取请求报文数据
        GET_REQUEST,    // 获得了完整的HTTP请求
        BAD_REQUEST,    // HTTP请求报文有语法错误
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR, // 服务器内部错误
        CLOSED_CONNECTION
    };
    enum LINE_STATUS {
        LINE_OK = 0, // 完整读取一行
        LINE_BAD,    // 报文语法有误
        LINE_OPEN    // 读取的行不完整
    };

  public:
    HttpConn() {}
    ~HttpConn() {}

  public:
    void init(int sockfd, const sockaddr_in &addr);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address() { return &address_; }
    void initmysql_result(ConnPool *connPool);

  private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return read_buf_ + start_line_; };
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

  public:
    static int epfd_;
    static int user_count_;
    MYSQL *mysql;

  private:
    int sockfd_;
    sockaddr_in address_;
    char read_buf_[READ_BUFFER_SIZE];
    int read_idx_;
    int checked_idx_;
    int start_line_;
    char write_buf_[WRITE_BUFFER_SIZE];
    int write_idx_;
    CHECK_STATE check_state_;
    METHOD method_;
    char real_file_[FILENAME_LEN];
    char *url_;
    char *version_;
    char *host_;
    int content_length_;
    bool linger_;
    char *file_address_;
    struct stat file_stat_;
    struct iovec iv_[2];
    int iv_count_;
    int cgi;       // 是否启用的POST
    char *string_; // 存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
};

#endif /* HTTPCONN_H */
