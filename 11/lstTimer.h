/**
 * @file lstTimer.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-08
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#pragma once
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <functional>
const int BUFFER_SIZE = 64;

class utilTimer;

struct clientData {
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    utilTimer* timer;
};

class utilTimer {
public:
    utilTimer() : prev(nullptr), next(nullptr) { }
public:
    time_t expire;
    std::function<void(clientData*)> cb_func;
    clientData* userData;
    utilTimer* prev, *next;
};

class sortTimerList {
public:
    sortTimerList() : head(new utilTimer()), tail(new utilTimer()) {         // 虚拟头结点
        head->next = tail;
        tail->prev = head;
    }
    ~sortTimerList() {
        utilTimer* tmp = head;
        while (tmp) {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }
    void addTimer(utilTimer* timer) {
        if (!timer) {
            return;
        }
        if (head->next == tail) {
            head->next = timer;
            tail->prev = timer;
            timer->next = tail;
            timer->prev = head;
            return;
        }
        addTimer(timer, head);
    }

    void adjustTimer(utilTimer* timer) {
        if (!timer) {
            return;
        }
        utilTimer* tmp = timer->next;
        if (tmp == tail || timer->expire < tmp->expire) {
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        addTimer(timer, timer->next);
    }

    void delTimer(utilTimer* timer) {
        if (!timer) {
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    void tick() {
        if (head->next == tail) {
            return;
        }
        printf("timer tick!\n");
        time_t cur = time(NULL);
        utilTimer* tmp = head->next;
        while (tmp != tail) {
            if (cur < tmp->expire) {
                break;
            }
            tmp->cb_func(tmp->userData);
            head->next = tmp->next;
            delete tmp;
            tmp = head->next;                  
        }
    }
private:
    void addTimer(utilTimer* timer, utilTimer* lst_head) {
        utilTimer* prev = lst_head;
        utilTimer* tmp = prev->next;
        while (tmp != tail && timer->expire >= tmp->expire) {
            prev = tmp;
            tmp = tmp->next;
        }
        prev->next = timer;
        timer->next = tmp;
        tmp->prev = timer;
        timer->prev = prev;
    }
    utilTimer* head, *tail;
};
