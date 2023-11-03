#ifndef SYNC_H
#define SYNC_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class Semaphore {
  public:
    Semaphore() {
        if (sem_init(&semaphre_, 0, 0) != 0) {
            throw std::exception();
        }
    }
    Semaphore(int num) {
        if (sem_init(&semaphre_, 0, num) != 0) {
            throw std::exception();
        }
    }
    ~Semaphore() { sem_destroy(&semaphre_); }
    bool wait() { return sem_wait(&semaphre_) == 0; }
    bool post() { return sem_post(&semaphre_) == 0; }

  private:
    sem_t semaphre_;
};
class MutexLock {
  public:
    MutexLock() {
        if (pthread_mutex_init(&mutex_, NULL) != 0) {
            throw std::exception();
        }
    }
    ~MutexLock() { pthread_mutex_destroy(&mutex_); }
    bool lock() { return pthread_mutex_lock(&mutex_) == 0; }
    bool unlock() { return pthread_mutex_unlock(&mutex_) == 0; }
    pthread_mutex_t *get() { return &mutex_; }

  private:
    pthread_mutex_t mutex_;
};
class Condition {
  public:
    Condition() {
        if (pthread_cond_init(&cond_, NULL) != 0) {
            throw std::exception();
        }
    }
    ~Condition() { pthread_cond_destroy(&cond_); }
    bool wait(pthread_mutex_t *m_mutex) {
        int ret = 0;
        ret = pthread_cond_wait(&cond_, m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t) {
        int ret = 0;
        ret = pthread_cond_timedwait(&cond_, m_mutex, &t);
        return ret == 0;
    }
    bool signal() { return pthread_cond_signal(&cond_) == 0; }
    bool broadcast() { return pthread_cond_broadcast(&cond_) == 0; }

  private:
    pthread_cond_t cond_;
};
#endif /* SYNC_H */
