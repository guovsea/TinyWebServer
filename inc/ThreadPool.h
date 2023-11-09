#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "ConnPool.h"
#include "sync.h"
#include <cstdio>
#include <exception>
#include <list>
#include <pthread.h>

template <typename T> class ThreadPool {
  public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    ThreadPool(ConnPool *connPool, int thread_number = 8,
               int max_request = 10000);
    ~ThreadPool();
    bool append(T *request);

  private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

  private:
    int thread_number_; // 线程池中的线程数
    int queue_capacity_;  // 请求队列中允许的最大请求数
    pthread_t *threads_; // 描述线程池的数组，其大小为m_thread_number
    std::list<T *> queue_; // 请求队列
    MutexLock lock_;    // 保护请求队列的互斥锁
    Semaphore queue_not_empty;      // 是否有任务需要处理
    bool is_stop_;                // 是否结束线程
    ConnPool *conn_pool_;       // 数据库
};
template <typename T>
ThreadPool<T>::ThreadPool(ConnPool *connPool, int thread_number,
                          int queue_capacity)
    : thread_number_(thread_number), queue_capacity_(queue_capacity), is_stop_(false),
      threads_(NULL), conn_pool_(connPool) {
    if (thread_number <= 0 || queue_capacity <= 0)
        throw std::exception();
    threads_ = new pthread_t[thread_number_];
    if (!threads_)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i) {
        if (pthread_create(threads_ + i, NULL, worker, this) != 0) {
            delete[] threads_;
            throw std::exception();
        }
        if (pthread_detach(threads_[i])) {
            delete[] threads_;
            throw std::exception();
        }
    }
}
template <typename T> ThreadPool<T>::~ThreadPool() {
    delete[] threads_;
    is_stop_ = true;
}
template <typename T> bool ThreadPool<T>::append(T *request) {
    lock_.lock();
    if (queue_.size() > queue_capacity_) {
        lock_.unlock();
        return false;
    }
    queue_.push_back(request);
    lock_.unlock();
    queue_not_empty.post();
    return true;
}
template <typename T> void *ThreadPool<T>::worker(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;
    pool->run();
    return pool;
}
template <typename T> void ThreadPool<T>::run() {
    while (!is_stop_) {
        queue_not_empty.wait();
        lock_.lock();
        if (queue_.empty()) {
            lock_.unlock();
            continue;
        }
        T *request = queue_.front();
        queue_.pop_front();
        lock_.unlock();
        if (!request)
            continue;

        ConnRAII(&request->mysql_, conn_pool_);

        request->process();
    }
}
#endif /* THREADPOOL_H */
