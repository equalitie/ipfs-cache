#pragma once

#include <event2/event_struct.h>
#include <chrono>
#include <functional>

namespace ipfs_cache {

class Timer {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;

public:
    Timer(struct event_base*);

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    void start(Duration, std::function<void()>);

    bool is_running() const;

    void stop();

    ~Timer();

private:
    static void call(evutil_socket_t, short, void *);

private:
    struct event_base* _evbase;
    event* _event = nullptr;
    std::function<void()> _cb;
};

} // ipfs_cache namespace
