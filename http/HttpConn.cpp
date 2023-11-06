#include "../log/log.h"
#include "HttpConn.h"
#include <fstream>
#include <map>
#include <mysql/mysql.h>

// #define connfdET //边缘触发非阻塞
#define connfdLT // 水平触发阻塞

// #define listenfdET //边缘触发非阻塞
#define listenfdLT // 水平触发阻塞

// 定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form =
    "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form =
    "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form =
    "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form =
    "There was an unusual problem serving the request file.\n";

// 当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
const char *doc_root = "/home/qgy/github/TinyWebServer/root";

// 将表中的用户名和密码放入map
map<string, string> users;
MutexLock lock_;

void HttpConn::initmysql_result(ConnPool *connPool) {
    // 先从连接池中取一个连接
    MYSQL *mysql = NULL;
    ConnRAII mysqlcon(&mysql, connPool);

    // 在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user")) {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    // 从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    // 返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    // 从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

// 对文件描述符设置非阻塞
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将 fd  添加到 epfd 的监听集合中, 是否开启 EPOLLONESHOT
void addfd(int epfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef listenfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef listenfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 从内核时间表删除描述符
void removefd(int epfd, int fd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int epfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
}

int HttpConn::user_count_ = 0;
int HttpConn::epfd_ = -1;

// 关闭连接，关闭一个连接，客户总量减一
void HttpConn::close_conn(bool real_close) {
    if (real_close && (sockfd_ != -1)) {
        removefd(epfd_, sockfd_);
        sockfd_ = -1;
        user_count_--;
    }
}

// 初始化连接,外部调用初始化套接字地址
void HttpConn::init(int sockfd, const sockaddr_in &addr) {
    sockfd_ = sockfd;
    address_ = addr;
    // int reuse=1;
    // setsockopt(sockfd_,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    addfd(epfd_, sockfd, true);
    user_count_++;
    init();
}

// 初始化新接受的连接
// check_state默认为分析请求行状态
void HttpConn::init() {
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    check_state_ = CHECK_STATE_REQUESTLINE;
    linger_ = false;
    method_ = GET;
    url_ = 0;
    version_ = 0;
    content_length_ = 0;
    host_ = 0;
    start_line_ = 0;
    checked_idx_ = 0;
    read_idx_ = 0;
    write_idx_ = 0;
    cgi = 0;
    memset(read_buf_, '\0', READ_BUFFER_SIZE);
    memset(write_buf_, '\0', WRITE_BUFFER_SIZE);
    memset(real_file_, '\0', FILENAME_LEN);
}

// 从状态机，用于分析出一行内容
// 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN(不完整)
HttpConn::LINE_STATUS HttpConn::parse_line() {
    char temp;
    for (; checked_idx_ < read_idx_; ++checked_idx_) {
        temp = read_buf_[checked_idx_];
        if (temp == '\r') {
            if ((checked_idx_ + 1) == read_idx_)
                return LINE_OPEN;
            else if (read_buf_[checked_idx_ + 1] == '\n') {
                read_buf_[checked_idx_++] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') {
            if (checked_idx_ > 1 && read_buf_[checked_idx_ - 1] == '\r') {
                read_buf_[checked_idx_ - 1] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 循环读取客户数据，直到无数据可读或对方关闭连接
// 非阻塞ET工作模式下，需要一次性将数据读完
bool HttpConn::read_once() {
    if (read_idx_ >= READ_BUFFER_SIZE) {
        return false;
    }
    int bytes_read = 0;

#ifdef connfdLT

    bytes_read =
        recv(sockfd_, read_buf_ + read_idx_, READ_BUFFER_SIZE - read_idx_, 0);

    if (bytes_read <= 0) {
        return false;
    }

    read_idx_ += bytes_read;

    return true;

#endif

#ifdef connfdET
    while (true) {
        bytes_read = recv(sockfd_, read_buf_ + read_idx_,
                          READ_BUFFER_SIZE - read_idx_, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        } else if (bytes_read == 0) {
            return false;
        }
        read_idx_ += bytes_read;
    }
    return true;
#endif
}

// 解析http请求行，获得请求方法，目标url及http版本号
HttpConn::HTTP_CODE HttpConn::parse_request_line(char *text) {
    url_ = strpbrk(text, " \t");
    if (!url_) {
        return BAD_REQUEST;
    }
    *url_++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        method_ = GET;
    else if (strcasecmp(method, "POST") == 0) {
        method_ = POST;
        cgi = 1;
    } else
        return BAD_REQUEST;
    url_ += strspn(url_, " \t");
    version_ = strpbrk(url_, " \t");
    if (!version_)
        return BAD_REQUEST;
    *version_++ = '\0';
    version_ += strspn(version_, " \t");
    if (strcasecmp(version_, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(url_, "http://", 7) == 0) {
        url_ += 7;
        url_ = strchr(url_, '/');
    }

    if (strncasecmp(url_, "https://", 8) == 0) {
        url_ += 8;
        url_ = strchr(url_, '/');
    }

    if (!url_ || url_[0] != '/')
        return BAD_REQUEST;
    // 当url为/时，显示判断界面
    if (strlen(url_) == 1)
        strcat(url_, "judge.html");
    check_state_ = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

// 解析http请求的一个头部信息
HttpConn::HTTP_CODE HttpConn::parse_headers(char *text) {
    if (text[0] == '\0') {
        if (content_length_ != 0) {
            check_state_ = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            linger_ = true;
        }
    } else if (strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        content_length_ = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        host_ = text;
    } else {
        // printf("oop!unknow header: %s\n",text);
        LOG_INFO("unknow header: %s", text);
        Log::instance()->flush();
    }
    return NO_REQUEST;
}

// 判断http请求是否被完整读入
HttpConn::HTTP_CODE HttpConn::parse_content(char *text) {
    if (read_idx_ >= (content_length_ + checked_idx_)) {
        text[content_length_] = '\0';
        // POST请求中最后为输入的用户名和密码
        string_ = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

//
HttpConn::HTTP_CODE HttpConn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    // 状态为解析消息体时，因为消息体不会以"\r\n"结尾。因此，parse_line()不会返回 LINE_OK
    // 应该使用上一次 parse_line()的结果。即：在解析Header时只要 parse_line == LINE_OK 就继续解析
    while ((check_state_ == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
           ((line_status = parse_line()) == LINE_OK)) {
        text = get_line();
        start_line_ = checked_idx_;
        LOG_INFO("%s", text);
        Log::instance()->flush();
        switch (check_state_) {
        case CHECK_STATE_REQUESTLINE: {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER: {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST) {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT: {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::do_request() {
    strcpy(real_file_, doc_root);
    int len = strlen(doc_root);
    // printf("url_:%s\n", url_);
    const char *p = strrchr(url_, '/');

    // 处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {

        // 根据标志判断是登录检测还是注册检测
        char flag = url_[1];

        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/");
        strcat(url_real, url_ + 2);
        strncpy(real_file_ + len, url_real, FILENAME_LEN - len - 1);
        free(url_real);

        // 将用户名和密码提取出来
        // user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; string_[i] != '&'; ++i)
            name[i - 5] = string_[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; string_[i] != '\0'; ++i, ++j)
            password[j] = string_[i];
        password[j] = '\0';

        // 同步线程登录校验
        if (*(p + 1) == '3') {
            // 如果是注册，先检测数据库中是否有重名的
            // 没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end()) {

                lock_.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                lock_.unlock();

                if (!res)
                    strcpy(url_, "/log.html");
                else
                    strcpy(url_, "/registerError.html");
            } else
                strcpy(url_, "/registerError.html");
        }
        // 如果是登录，直接判断
        // 若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2') {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(url_, "/welcome.html");
            else
                strcpy(url_, "/logError.html");
        }
    }

    if (*(p + 1) == '0') {
        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/register.html");
        strncpy(real_file_ + len, url_real, strlen(url_real));

        free(url_real);
    } else if (*(p + 1) == '1') {
        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/log.html");
        strncpy(real_file_ + len, url_real, strlen(url_real));

        free(url_real);
    } else if (*(p + 1) == '5') {
        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/picture.html");
        strncpy(real_file_ + len, url_real, strlen(url_real));

        free(url_real);
    } else if (*(p + 1) == '6') {
        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/video.html");
        strncpy(real_file_ + len, url_real, strlen(url_real));

        free(url_real);
    } else if (*(p + 1) == '7') {
        char *url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(url_real, "/fans.html");
        strncpy(real_file_ + len, url_real, strlen(url_real));

        free(url_real);
    } else
        strncpy(real_file_ + len, url_, FILENAME_LEN - len - 1);

    if (stat(real_file_, &file_stat_) < 0)
        return NO_RESOURCE;
    if (!(file_stat_.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    if (S_ISDIR(file_stat_.st_mode))
        return BAD_REQUEST;
    int fd = open(real_file_, O_RDONLY);
    file_address_ =
        (char *)mmap(0, file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}
void HttpConn::unmap() {
    if (file_address_) {
        munmap(file_address_, file_stat_.st_size);
        file_address_ = 0;
    }
}

bool HttpConn::write() {
    int temp = 0;

    if (bytes_to_send == 0) {
        modfd(epfd_, sockfd_, EPOLLIN);
        init();
        return true;
    }

    while (1) {
        temp = writev(sockfd_, iv_, iv_count_);

        if (temp < 0) {
            if (errno == EAGAIN) {
                modfd(epfd_, sockfd_, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        // 根据写入的字节数，更新iov
        if (bytes_have_send >= iv_[0].iov_len) {
            iv_[0].iov_len = 0;
            iv_[1].iov_base = file_address_ + (bytes_have_send - write_idx_);
            iv_[1].iov_len = bytes_to_send;
        } else {
            iv_[0].iov_base = write_buf_ + bytes_have_send;
            iv_[0].iov_len = iv_[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0) {
            unmap();
            modfd(epfd_, sockfd_, EPOLLIN);

            if (linger_) {
                init();
                return true;
            } else {
                return false;
            }
        }
    }
}

bool HttpConn::add_response(const char *format, ...) {
    if (write_idx_ >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(write_buf_ + write_idx_,
                        WRITE_BUFFER_SIZE - 1 - write_idx_, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - write_idx_)) {
        va_end(arg_list);
        return false;
    }
    write_idx_ += len;
    va_end(arg_list);
    LOG_INFO("request:%s", write_buf_);
    Log::instance()->flush();
    return true;
}
bool HttpConn::add_status_line(int status, const char *title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool HttpConn::add_headers(int content_len) {
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}
bool HttpConn::add_content_length(int content_len) {
    return add_response("Content-Length:%d\r\n", content_len);
}
bool HttpConn::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool HttpConn::add_linger() {
    return add_response("Connection:%s\r\n",
                        (linger_ == true) ? "keep-alive" : "close");
}
bool HttpConn::add_blank_line() { return add_response("%s", "\r\n"); }
bool HttpConn::add_content(const char *content) {
    return add_response("%s", content);
}
bool HttpConn::process_write(HTTP_CODE ret) {
    switch (ret) {
    case INTERNAL_ERROR: {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST: {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST: {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST: {
        add_status_line(200, ok_200_title);
        if (file_stat_.st_size != 0) {
            add_headers(file_stat_.st_size);
            iv_[0].iov_base = write_buf_;
            iv_[0].iov_len = write_idx_;
            iv_[1].iov_base = file_address_;
            iv_[1].iov_len = file_stat_.st_size;
            iv_count_ = 2;
            bytes_to_send = write_idx_ + file_stat_.st_size;
            return true;
        } else {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    iv_[0].iov_base = write_buf_;
    iv_[0].iov_len = write_idx_;
    iv_count_ = 1;
    bytes_to_send = write_idx_;
    return true;
}
void HttpConn::process() {
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST) {
        // 请求不完整，需要继续读取请求报文数据
        modfd(epfd_, sockfd_, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret) {
        close_conn();
    }
    modfd(epfd_, sockfd_, EPOLLOUT);
}
