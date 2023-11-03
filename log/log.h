#ifndef LOG_H
#define LOG_H

#include "block_queue.h"
#include <cstdio>
#include <iostream>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>

#define LOG_DIR_LEN 128
#define LOG_NAME_LEN 128

using namespace std;

class Log {
  public:
    // local static 实现单例模式,C++11 后最优雅的实现
    static Log *instance() {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args) {
        Log::instance()->async_write_log();
        return NULL;
    }
    // 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    /**
     * @brief 初始化 Log 对象
     *
     * @param file_name         日志文件
     * @param log_buf_size      日志缓冲区大小，默认4KB
     * @param split_lines       最大日志行数
     * @param queue_capacity    阻塞队列的容量,如果设置不为0,则采用异步模式
     * @return true
     * @return false
     */
    bool init(const char *file_name, int log_buf_size = 8192,
              int split_lines = 5000000, int queue_capacity = 0);

    void write_log(int level, const char *format, ...);

    void flush(void);

  private:
    Log();
    virtual ~Log();
    void *async_write_log() {
        string single_log;
        // 从阻塞队列中取出一个日志string，写入文件
        while (log_queue_->pop(single_log)) {
            mutex_.lock();
            fputs(single_log.c_str(), fp_);
            mutex_.unlock();
        }
        return NULL;
    }

  private:
    char dir_name[LOG_DIR_LEN];  // 路径名
    char log_name[LOG_NAME_LEN]; // log文件名
    int split_lines_;            // 单个日志文件最大行数
    int log_buf_size_;           // 日志缓冲区大小
    long long count_;            // 日志行数记录
    int today_;                  // 按天分类,记录当前时间是那一天
    FILE *fp_;                   // 打开log的文件指针
    char *buf_;
    block_queue<string> *log_queue_; // 阻塞队列
    bool is_async_;                  // 是否异步标志位
    MutexLock mutex_;
};

#define LOG_DEBUG(format, ...)                                                 \
    Log::instance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)                                                  \
    Log::instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)                                                  \
    Log::instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...)                                                 \
    Log::instance()->write_log(3, format, ##__VA_ARGS__)

#endif
