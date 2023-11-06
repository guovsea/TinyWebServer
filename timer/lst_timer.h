#ifndef LST_TIMER
#define LST_TIMER

#include "../log/log.h"
#include <arpa/inet.h>
#include <time.h>

class Timer;
struct client_data {
    sockaddr_in address;
    int sockfd;
    Timer *timer;
};

class Timer {
  public:
    Timer() : prev(NULL), next(NULL) {}

  public:
    time_t expire;                  // 失效时间
    void (*cb_func)(client_data *); // 超时动作
    client_data *user_data;         // 数据
    Timer *prev;
    Timer *next;
};

// 按升序排列的timer链表
class SortedTmList {
  public:
    SortedTmList() : head_(NULL), tail_(NULL) {}
    ~SortedTmList() {
        Timer *tmp = head_;
        while (tmp) {
            head_ = tmp->next;
            delete tmp;
            tmp = head_;
        }
    }
    // 添加 timer
    void add_timer(Timer *timer) {
        if (!timer) {
            return;
        }
        if (!head_) {
            head_ = tail_ = timer;
            return;
        }
        if (timer->expire < head_->expire) {
            timer->next = head_;
            head_->prev = timer;
            head_ = timer;
            return;
        }
        insert_timer(timer, head_);
    }
    void flush_timer(Timer *timer) {
        if (!timer) {
            return;
        }
        Timer *tmp = timer->next;
        if (!tmp || (timer->expire < tmp->expire)) {
            return;
        }
        if (timer == head_) {
            head_ = head_->next;
            head_->prev = NULL;
            timer->next = NULL;
            insert_timer(timer, head_);
        } else {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            insert_timer(timer, timer->next);
        }
    }
    void del_timer(Timer *timer) {
        if (!timer) {
            return;
        }
        if ((timer == head_) && (timer == tail_)) {
            delete timer;
            head_ = NULL;
            tail_ = NULL;
            return;
        }
        if (timer == head_) {
            head_ = head_->next;
            head_->prev = NULL;
            delete timer;
            return;
        }
        if (timer == tail_) {
            tail_ = tail_->prev;
            tail_->next = NULL;
            delete timer;
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }
    // 唤醒所有已经失效的 Timer
    void tick() {
        if (!head_) {
            return;
        }
        LOG_INFO("%s", "timer tick");
        Log::instance()->flush();
        time_t cur = time(NULL);
        Timer *tmp = head_;
        while (tmp) {
            if (cur < tmp->expire) {
                break;
            }
            tmp->cb_func(tmp->user_data);
            head_ = tmp->next;
            if (head_) {
                head_->prev = NULL;
            }
            delete tmp;
            tmp = head_;
        }
    }

  private:
    void insert_timer(Timer *timer, Timer *ls_head) {
        Timer *prev = ls_head;
        Timer *tmp = prev->next;
        while (tmp) {
            if (timer->expire < tmp->expire) {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        if (!tmp) {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail_ = timer;
        }
    }

  private:
    Timer *head_;
    Timer *tail_;
};

#endif
