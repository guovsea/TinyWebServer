#if 0
#include <iostream>
#include "threadpool.h"

void run(void*arg) {
    std::cout << "thread " << *((int*)arg) << "is running" << std::endl;
}


int main(int argc, char const *argv[])
{
    ThreadPool<int> pool()
    return 0;
}

#endif