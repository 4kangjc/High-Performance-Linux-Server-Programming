#pragma once

#include <cstdio>
#include <exception>
#include <pthread.h>
#include <queue>
#include "locker.h"
#include <thread>

template<typename T>
class threadpoll {
public:
    threadpoll(size_t thread_number = std::thread::hardware_concurrency(), size_t max_requests = 1000);
    ~threadpoll();
    bool append(T* request);
private:
    static void* worker(void* arg);
    void run();
private:
    int thread_number;                          // 线程池中的线程数
    int max_requests;                           // 请求队列中允许的最大请求数
    std::vector<pthread_t> threads;             // 描述线程池的数组
    std::queue<T*> workqueue;                   // 请求队列
    locker queuelocker;                         // 保护请求队列的互斥锁
    sem queuestat;                              // 是否有任务需要处理
    bool stop = false;                          // 是否结束线程
};

template<typename T>
threadpoll<T>::threadpoll(size_t thread_number, size_t max_requests) : thread_number(thread_number), max_requests(max_requests) {
    for (int i = 0; i < thread_number; ++i) {
        printf("create the %dth thread\n", i);
        if (pthread_create(threads + i, NULL, worker, this) != 0) {
            throw std::exception();
        }
        if (pthread_detach(thread[i])) {
            throw std::exception();
        }
    }
}

template<typename T>
threadpoll<T>::~threadpoll() {
    stop = true;
}

template<typename T>
bool threadpoll<T>::append(T* request) {
    queuelocker.lock();
    if (workqueue.size() > max_requests) {
        queuelocker.unlock();
        return false;
    }
    workqueue.push(request);
    workqueue.unlock();
    queuestat.post();
    return true;
}

template<typename T>
void* threadpoll<T>::worker(void* arg) {
    threadpoll* poll = (threadpoll*)arg;
    poll->run();
    return poll;
}


template<typename T>
void threadpoll<T>::run() {
    while (!stop) {
        queuestat.wait();
        queuelocker.lock();
        if (workqueue.empty()) {
            queuelocker.unlock();
            continue;
        }
        T* request = workqueue.front();
        workqueue.pop();
        queuelocker.unlock();
        if (!request) {
            continue;
        }
        request->process();
    }
}