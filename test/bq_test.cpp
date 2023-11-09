#include "BlockQueue.h"
#include <iostream>
#include <pthread.h>
#include <unistd.h>

BlockQueue<int> bq(10);

void *run(void *) {
    sleep(1); // 先让main 线程添加元素
    int i;
    while (true) {
        bq.pop(i);
        std::cout << "pop " << i << std::endl;
    }
}

int main() {
    pthread_t tid;
    pthread_create(&tid, NULL, run, NULL);
    for (int i = 0; i < 100000; i++) {
        bq.push(i);
        std::cout << "push " << i << std::endl;
    }
    pthread_join(tid, NULL);

    return 0;
}
