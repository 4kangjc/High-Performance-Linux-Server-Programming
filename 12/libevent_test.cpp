/**
 * @file libevent_test.cpp
 * @author kangjinci (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include <sys/signal.h>
#include <event.h>

void signal_cb(int fd, short event, void* argc) {
    event_base* base = (event_base*)argc;
    timeval delay = {2, 0};
    printf("Caught an interrupt signal; exiting cleanly in two seconds...\n");
    event_base_loopexit(base, &delay);
}

void timeout_cb(int fd, short event, void* argc) {
    printf("timeout...\n");
}


int main() {
    event_base* base = event_init();
    event* signal_event = evsignal_new(base, SIGINT, signal_cb, base);
    event_add(signal_event, nullptr);

    timeval tv = {1, 0};
    event* timeout_event = evtimer_new(base, timeout_cb, nullptr);
    event_add(timeout_event, &tv);

    event_base_dispatch(base);

    event_free(timeout_event);
    event_free(signal_event);
    event_base_free(base);

}
