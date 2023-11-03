#include "log.h"
#include <cstdio>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define DATE_FMT_LEN 16
#define FILE_NAME_LEN (LOG_DIR_LEN + LOG_NAME_LEN + DATE_FMT_LEN)
using namespace std;

Log::Log() {
    count_ = 0;
    is_async_ = false;
}

Log::~Log() {
    if (fp_ != NULL) {
        fclose(fp_);
    }
}

bool Log::init(const char *file_name, int log_buf_size, int split_lines,
               int queue_capacity) {

    // 如果设置了queue_capacity,则设置为异步
    if (queue_capacity >= 1) {
        is_async_ = true;
        log_queue_ = new block_queue<string>(queue_capacity);
        pthread_t tid;
        // flush_log_threadr 工作函数,创建线程异步写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    log_buf_size_ = log_buf_size;
    buf_ = new char[log_buf_size_];
    memset(buf_, '\0', log_buf_size_);
    split_lines_ = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm now = *sys_tm;

    const char *p = strrchr(file_name, '/');
    char log_full_name[FILE_NAME_LEN] = {0};
    char date[DATE_FMT_LEN] = {0};

    snprintf(date, 16, "%d_%02d_%02d_", now.tm_year + 1900, now.tm_mon + 1,
             now.tm_mday);

    // 拼接log文件名
    if (p == NULL) {
        // file_name 中如果不包含路径(/),则直接拼接为: 时间_file_name
        snprintf(log_full_name, FILE_NAME_LEN - 1, "%s%s", date, file_name);
    } else {
        // file_name
        // 中如果包含路径(/),则以最后一个/前面的字符作为dir_name,后面的字符串作为file_name
        // ,并拼接为 dir_name/日期_file_name
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, FILE_NAME_LEN - 1, "%s%s%s", dir_name, date,
                 log_name);
    }

    today_ = now.tm_mday;

    fp_ = fopen(log_full_name, "a");
    if (fp_ == NULL) {
        return false;
    }

    return true;
}

void Log::write_log(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level) {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info ]:");
        break;
    case 2:
        strcpy(s, "[warn ]:");
        break;
    case 3:
        strcpy(s, "[erro ]:");
        break;
    default:
        strcpy(s, "[info ]:");
        break;
    }
    mutex_.lock();
    count_++; // 写入日志的总行数

    // 如果日志记录日期和当前日期不相等,或者打到每个日志文件的最大记录条数,则新建一个日志文件
    if (today_ != my_tm.tm_mday || count_ % split_lines_ == 0) {

        char new_log[FILE_NAME_LEN] = {0};
        // 先将fp_在标准IO缓冲中的内容强制刷入磁盘
        fflush(fp_);
        fclose(fp_);
        char date[DATE_FMT_LEN] = {0};

        snprintf(date, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900,
                 my_tm.tm_mon + 1, my_tm.tm_mday);

        if (today_ != my_tm.tm_mday) {
            snprintf(new_log, FILE_NAME_LEN, "%s%s%s", dir_name, date,
                     log_name);
            today_ = my_tm.tm_mday;
            count_ = 0;
        } else {
            snprintf(new_log, FILE_NAME_LEN, "%s%s%s.%lld", dir_name, date,
                     log_name, count_ / split_lines_);
        }
        fp_ = fopen(new_log, "a");
    }

    mutex_.unlock();

    va_list valst;
    va_start(valst, format);

    string log_str;
    mutex_.lock();

    // 写入的具体时间内容格式 48 = 4+1+2+1+2+1+2+1+2+1+2+1+2+1+6+1+16+1+1
    int n = snprintf(buf_, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // 从 buf_ + n 处开始写入实际内容
    int m = vsnprintf(buf_ + n, log_buf_size_ - n - 1, format, valst);
    buf_[n + m] = '\n';
    buf_[n + m + 1] = '\0';
    log_str = buf_;

    mutex_.unlock();
    va_end(valst);

    if (is_async_ && !log_queue_->full()) {
        log_queue_->push(log_str);
    } else {
        mutex_.lock();
        fputs(log_str.c_str(), fp_);
        mutex_.unlock();
    }
}

void Log::flush(void) {
    mutex_.lock();
    // 强制将标准IO缓冲中的内容刷入磁盘中
    fflush(fp_);
    mutex_.unlock();
}

#if 0

int main(int argc, char *argv[]) {
    Log *p = Log::instance();
    p->init("test");
    for (int i = 0; i < 10; i++) {
        LOG_DEBUG("int i = %d", i);
        LOG_INFO("int i = %d", i);
        LOG_WARN("int i = %d", i);
        LOG_ERROR("int i = %d", i);
    }
    return 0;
}
#endif
