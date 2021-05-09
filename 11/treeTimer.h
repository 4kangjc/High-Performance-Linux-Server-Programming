/**
 * @file treeTimer.h
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-09
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#pragma once
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <functional>
#include <map>
#include <set>
#include <memory>
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
    utilTimer() = default;
    ~utilTimer() {
        cb_func = nullptr;
    }
    utilTimer(time_t expire, std::function<void(clientData*)> cb_func, clientData* userData) : expire(expire), cb_func(cb_func), userData(userData) { }
public:
    time_t expire;
    std::function<void(clientData*)> cb_func;
    clientData* userData;
};

class TreeTimerSet {
public:
    TreeTimerSet() = default;
    ~TreeTimerSet();

    void addTimer(utilTimer* timer);
    void adjustTimer(utilTimer* timer);
    void delTimer(utilTimer* timer);
    void tick();
private:
    //using Node = std::shared_ptr<utilTimer>;
    struct cmp {
        bool operator()(const utilTimer* lhs, const utilTimer* rhs) const {
            return lhs->expire < rhs->expire;
        }
    };
    std::set<utilTimer*, cmp> st;
};

void TreeTimerSet::addTimer(utilTimer* timer) {
    st.insert(timer);
}

void TreeTimerSet::adjustTimer(utilTimer* timer) {
    st.erase(st.find(timer));
    st.insert(timer);
}
void TreeTimerSet::delTimer(utilTimer* timer) {
    st.erase(st.find(timer));
    delete timer;
}
void TreeTimerSet::tick() {
    if (st.empty()) {
        return;
    }
    printf("timer tick!\n");
    time_t cur = time(NULL);
    auto it = st.begin();
    while (it != st.end() && cur >= (*it)->expire) {
        auto pre = *it;
        pre->cb_func(pre->userData);
        ++it;
        st.erase(prev(it));
        delete(pre);
    }
}

TreeTimerSet::~TreeTimerSet() {
    auto it = st.begin();
    while (it != st.end()) {
        auto pre = *it;
        ++it;
        st.erase(prev(it));
        delete(pre);
    }
}
