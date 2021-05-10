/**
 * @file timeHeap.h
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#pragma once
#include <iostream>
#include <queue>
#include <functional>
#include <netinet/in.h>
#include <ctime>

static const int BUFFER_SIZE = 64;

class heapTimer;

struct clientData {
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    heapTimer* timer;
};

class heapTimer {
public:
    heapTimer(int delay) : expire(time(NULL) + delay) { }
    ~heapTimer() {
        userData = nullptr;
    }
public:
    time_t expire;
    std::function<void(clientData*)> cb_func;
    clientData* userData;
};

class timeHeap {
public:
    timeHeap(const std::vector<heapTimer*>& array) : c(array) {
        int N = c.size();
		for (int i = (N - 1) / 2; i >= 0; --i) {
			sink(i);
		}
    }
    timeHeap(heapTimer** init_array, int size, int capacity) {
        std::vector<heapTimer*> array(size);
        array.reserve(capacity);
        for (int i = 0; i < size; ++i) {
            array[i] = init_array[i];
        }
        int N = c.size();
		for (int i = (N - 1) / 2; i >= 0; --i) {
			sink(i);
		}
    }
    timeHeap() {
        c.reserve(10);
    }
    ~timeHeap() {
        int n = c.size();
        for (int i = 0; i < n; ++i) {
            delete c[i];
            c[i] = nullptr;
        }
        c.clear();
    }
private:
	void swim(int k) {
		while (k > 0 && comp(c[(k - 1) / 2], c[k])) {
			std::swap(c[(k - 1) / 2], c[k]);
			k = (k - 1) / 2;
		}
	}
	void sink(int k) {
		int N = c.size();
		while (2 * k + 1 < N) {
			int j = 2 * k + 1;
			if (j + 1 < N && comp(c[j], c[j + 1])) {
				j++;
			}
			if (!comp(c[k], c[j])) {
				break;
			}
			std::swap(c[k], c[j]);
			k = j;
		}
	}
    static bool comp(heapTimer* lhs, heapTimer* rhs) noexcept {
        return lhs->expire < rhs->expire;
    }
public:
	inline bool empty() const { return c.size() == 0; }
private:
	void push(heapTimer* v) {
		int N = c.size();
		c.push_back(v);
		swim(N);
    }
    void pop() {
		std::swap(c[0], c[c.size() - 1]);
        if (c[c.size() - 1] != nullptr) {
            delete c[c.size() - 1];
            c[c.size() - 1] = nullptr;
        }
		c.pop_back();
		sink(0);
	}
public:
    void addTimer(heapTimer* timer) {
        if (!timer) {
            return;
        }
        push(timer);
    }
    void delTimer(heapTimer* timer) {
        if (!timer) {
            return;
        }
        timer->cb_func = nullptr;
    }
    void popTimer() {
        if (!empty())
            pop();
    }
    void tick() {
        heapTimer* tmp = c[0];
        time_t cur = time(NULL);
        while (!c.empty()) {
            if (!tmp) {
                break;
            }
            if (tmp->expire > cur) {
                break;
            }
            if (c[0]->cb_func) {
                c[0]->cb_func(c[0]->userData);
            }
            popTimer();
            tmp = c[0];
        }
    }

    const heapTimer* top() noexcept {
        if (empty()) {
            return nullptr;
        }
		return c.front();
	}
	
private:
    std::vector<heapTimer*> c{};
};
