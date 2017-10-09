#include "timer.h"
#include <boost/asio/steady_timer.hpp>

using namespace std;
using namespace ipfs_cache;

namespace asio = boost::asio;
namespace sys  = boost::system;

struct Timer::Impl : public enable_shared_from_this<Impl> {
    asio::steady_timer timer;
    bool was_stopped = false;

    Impl(asio::io_service& ios)
        : timer(ios)
    {}
};

Timer::Timer(asio::io_service& ios)
    : _ios(ios)
{}

void Timer::start(Clock::duration duration, std::function<void()> cb)
{
    stop();

    _impl = make_shared<Impl>(_ios);
    _impl->timer.expires_from_now(duration);
    _impl->timer.async_wait([impl = _impl, cb = move(cb)] (sys::error_code) {
                                if (impl->was_stopped) return;
                                cb();
                            });
}

bool Timer::is_running() const
{
    return _impl != nullptr;
}

void Timer::stop()
{
    if (_impl) {
        _impl->was_stopped = true;
        _impl->timer.cancel();
        _impl = nullptr;
    }
}

Timer::~Timer()
{
    stop();
}
