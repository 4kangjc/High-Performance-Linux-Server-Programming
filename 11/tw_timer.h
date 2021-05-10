/**
 * @file tw_timer.h
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#pragma once

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <functional>

const int BUFFER_SIZE = 64;
class tw_timer;

struct client_data {
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    tw_timer* timer;
};

class tw_timer {
public:
    tw_timer(int rot, int ts) : next(nullptr), prev(nullptr), rotation(rot), time_slot(ts) { }
public:
    int rotation;                                       // 记录定时器在时间轮转多少圈后生效
    int time_slot;                                      // 记录定时器属于时间轮上的哪个槽
    std::function<void(client_data*)> cb_func;          // 定时器的回调函数
    client_data* user_data;                            // 客户数据
    tw_timer* prev, *next;
};

class time_wheel {
public:
    time_wheel() : cur_slot(0) {
        for (int i = 0; i < N; ++i) {
            slots[i] = nullptr;
        }
    }
    ~time_wheel() {
        for (int i = 0; i < N; ++i) {
            tw_timer* tmp = slots[i];
            while (tmp) {
                slots[i] = tmp->next;
                delete tmp;
                tmp = slots[i];
            }
        }
    }
    tw_timer* add_timer(int timeout) {
        if (timeout < 0) {
            return nullptr;
        }
        int ticks = 0;
        if (timeout < TI) {
            ticks = 1;
        } else {
            ticks = timeout / TI;
        }
        int rotation = ticks / N;
        int ts = (cur_slot + (ticks % N)) % N;
        tw_timer* timer = new tw_timer(rotation, ts);
        if (!slots[ts]) {
            printf("add timer, rotation is %d, ts is %d, cur_slot is %d\n", rotation, ts, cur_slot);
            slots[ts] = timer;
        } else {
            timer->next = slots[ts];
            slots[ts]->prev = timer;                // 头插法
            slots[ts] = timer;
        }
        return timer;
    }
    void del_timer(tw_timer* timer) {
        if (!timer) {
            return;
        }
        int ts = timer->time_slot;
        if (timer == slots[ts]) {
            slots[ts] = slots[ts]->next;
            if (slots[ts]) {
                slots[ts]->prev = nullptr;
            }
            delete timer;
        } else {
            timer->prev->next = timer->next;
            if (timer->next) {
                timer->next->prev = timer->prev;
            }
            delete timer;
        }
    }

    void tick() {
        tw_timer* tmp = slots[cur_slot];
        printf("current slot is %d\n", cur_slot);
        while (tmp) {
            printf("tick the timer once\n");
            if (tmp->rotation > 0) {
                tmp->rotation--;
                tmp = tmp->next;
            } else {
                tmp->cb_func(tmp->user_data);
                if (tmp == slots[cur_slot]) {
                    slots[cur_slot] = tmp->next;
                    delete tmp;
                    if (slots[cur_slot]) {
                        slots[cur_slot]->prev = nullptr;
                    }
                    tmp = slots[cur_slot];
                } else {
                    tmp->prev->next = tmp->next;
                    if (tmp->next) {
                        tmp->next->prev = tmp->prev;
                    }
                    auto tmp2 = tmp->next;
                    delete tmp;
                    tmp = tmp2;
                }
            }
        }
        cur_slot = ++cur_slot % N;
    }
private:
    static const int N = 60;
    static const int TI = 1; 
    tw_timer* slots[N];
    int cur_slot;
};
