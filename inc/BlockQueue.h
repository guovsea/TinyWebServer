#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include "sync.h"
#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
using namespace std;

template <class T> class BlockQueue {
  public:
    BlockQueue(int max_size = 1000) {
        if (max_size <= 0) {
            exit(-1);
        }

        capacity_ = max_size;
        queue_ = new T[max_size];
        size_ = 0;
        front_ = -1;
        back_ = -1;
    }

    void clear() {
        mutex_.lock();
        size_ = 0;
        front_ = -1;
        back_ = -1;
        mutex_.unlock();
    }

    ~BlockQueue() {
        mutex_.lock();
        if (queue_ != NULL)
            delete[] queue_;

        mutex_.unlock();
    }

    bool full() {
        mutex_.lock();
        if (size_ >= capacity_) {
            mutex_.unlock();
            return true;
        }
        mutex_.unlock();
        return false;
    }

    bool empty() {
        mutex_.lock();
        if (0 == size_) {
            mutex_.unlock();
            return true;
        }
        mutex_.unlock();
        return false;
    }

    bool front(T &value) {
        mutex_.lock();
        if (0 == size_) {
            mutex_.unlock();
            return false;
        }
        value = queue_[front_];
        mutex_.unlock();
        return true;
    }

    bool back(T &value) {
        mutex_.lock();
        if (0 == size_) {
            mutex_.unlock();
            return false;
        }
        value = queue_[back_];
        mutex_.unlock();
        return true;
    }

    int size() {
        int tmp = 0;

        mutex_.lock();
        tmp = size_;

        mutex_.unlock();
        return tmp;
    }

    int max_size() {
        int tmp = 0;

        mutex_.lock();
        tmp = capacity_;

        mutex_.unlock();
        return tmp;
    }

    bool push(const T &item) {
        mutex_.lock();
        while (size_ >= capacity_) {
            queue_not_full_.wait(mutex_.get());
        }

        back_ = (back_ + 1) % capacity_;
        queue_[back_] = item;

        size_++;

        queue_not_empty_.signal();
        mutex_.unlock();
        return true;
    }

    bool pop(T &item) {
        mutex_.lock();
        while (size_ <= 0) {
            if (!queue_not_empty_.wait(mutex_.get())) {
                return false;
            }
        }

        front_ = (front_ + 1) % capacity_;
        item = queue_[front_];
        size_--;

        queue_not_full_.signal();
        mutex_.unlock();
        return true;
    }

    bool pop(T &item, int ms_timeout) {
        struct timespec time_gap = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        mutex_.lock();
        if (size_ <= 0) {
            time_gap.tv_sec = now.tv_sec + ms_timeout / 1000;
            time_gap.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!queue_not_empty_.timewait(mutex_.get(), time_gap)) {
                mutex_.unlock();
                return false;
            }
        }

        if (size_ <= 0) {
            mutex_.unlock();
            return false;
        }

        front_ = (front_ + 1) % capacity_;
        item = queue_[front_];
        size_--;

        queue_not_full_.signal();
        mutex_.unlock();
        return true;
    }

  private:
    MutexLock mutex_;
    Condition queue_not_empty_; // pop 时需要等待队列不空的条件成立
    Condition queue_not_full_; // push 时需要等待队列不满的条件成立

    T *queue_;
    int size_;
    int capacity_;
    int front_;
    int back_;
};

#endif /* BLOCKQUEUE_H */
