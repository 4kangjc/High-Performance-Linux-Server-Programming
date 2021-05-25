/**
 * @file locker.h
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-23
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#pragma once
#include <exception>
#include <pthread.h>
#include <semaphore.h>
#include <functional>

class sem {
public:
    sem() {
        if (sem_init(&m_sem, 0, 0) != 0) {
            throw std::exception();
        }
    }
    ~sem() {
        sem_destroy(&m_sem);
    }
    bool wait() {
        return sem_wait(&m_sem) == 0;
    }
    bool post() {
        return sem_post(&m_sem);
    }
private:
    sem_t m_sem;
};

class locker {
public:
    locker() {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }
    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    friend class cond;
private:
    pthread_mutex_t m_mutex;
};

class cond {
public:
    cond() : m_mutex() {
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            m_mutex.~locker();
            throw std::exception();
        } 
    }
    ~cond() {
        pthread_cond_destroy(&m_cond);
    }
    bool wait(std::function<bool()> f) {                   // 条件
        int ret = 0;
        m_mutex.lock();
        while (!f()) {
            ret = pthread_cond_wait(&m_cond, &m_mutex.m_mutex);
        }
        m_mutex.unlock();
        return ret == 0;
    }
    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }
private:
    locker m_mutex;
    pthread_cond_t m_cond;
};