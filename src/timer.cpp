#include <ipfs_cache/timer.h>
#include <event2/event.h>

using namespace ipfs_cache;

Timer::Timer(struct event_base* evbase)
    : _evbase(evbase)
{ }

bool Timer::is_running() const
{
    return _event != nullptr;
}

void Timer::stop()
{
    if (!_event) return;
    evtimer_del(_event);
    event_free(_event);
    _event = nullptr;
    _cb = nullptr;
}

void Timer::start(Clock::duration duration, std::function<void()> cb)
{
    if (is_running()) {
        stop();
    }

    using namespace std::chrono;

    auto timeout_ms = duration_cast<milliseconds>(duration).count();

    struct timeval timeout;

    timeout.tv_sec  = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    _cb = std::move(cb);

    _event = event_new(_evbase, -1, 0, Timer::call, this);

    evtimer_add(_event, &timeout);
}

void Timer::call(evutil_socket_t, short, void * v)
{
    auto self = (Timer*) v;
    auto cb = std::move(self->_cb);
    self->stop();
    cb();
}

Timer::~Timer()
{
    stop();
}

