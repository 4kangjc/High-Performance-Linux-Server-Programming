#pragma once

#include <cstdio>
#include <exception>
#include <pthread.h>
#include <list>
#include "../14/locker.h"

template<typename T>
class threadpoll {
public:
    threadpoll(int thread_number = 8, int max_requests = 1000);
    ~threadpoll();
    bool append(T* request);
private:
    static void* worker(void* arg);
    void run();
private:
    int m_thread_number;                    // 线程池中的线程数
    int m_max_requests;                     // 请求队列中允许的最大请求数
    pthread_t* m_threads;                    // 描述线程池的数组
    std::list<T*> m_workqueue;              // 请求队列
    locker m_queuelocker;                   // 保护请求队列的互斥锁
    sem m_queuestat;                       // 是否有任务需要处理
    bool m_stop;                           // 是否结束线程
};

template<typename T>
threadpoll<T>::threadpoll(int thread_number, int max_requests) : m_thread_number(thread_number), 
    m_max_requests(max_requests), m_stop(false), m_threads(NULL) {
        if (thread_number <= 0 || max_requests <= 0) {
            throw std::exception();
        }
        m_threads = new pthread_t[m_thread_number];
        if (!m_threads) {
            throw std::exception();
        }
        for (int i = 0; i < thread_number; ++i) {
            printf("create the %dth thread\n", i);
            if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
                delete[] m_threads;
                throw std::exception();
            }
            if (pthread_detach(m_thread[i])) {
                delete[] m_threads;
                throw std::exception();
            }
        }
}

template<typename T>
threadpoll<T>::~threadpoll() {
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpoll<T>::append(T* request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_workqueue.unlock();
    m_queuestat.post();
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
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) {
            continue;
        }
        request->process();
    }
}