#include "republisher.h"
#include "backend.h"

using namespace std;
using namespace ipfs_cache;

static const Timer::Clock::duration publish_duration = chrono::minutes(10);

Republisher::Republisher(Backend& backend)
    : _was_destroyed(make_shared<bool>(false))
    , _backend(backend)
    , _timer(_backend.evbase())
{}

void Republisher::publish(const std::string& cid, std::function<void()> cb)
{
    _to_publish = cid;

    _callbacks.push_back(move(cb));

    if (_is_publishing) {
        return;
    }

    start_publishing();
}

void Republisher::start_publishing()
{
    if (_callbacks.empty()) {
        _timer.start(publish_duration / 2, [this] {
            _callbacks.push_back(nullptr);
            start_publishing();
        });
        return;
    }

    _timer.stop();

    _is_publishing = true;

    auto last_i = --_callbacks.end();

    _backend.publish(_to_publish, publish_duration,
        [this, d = _was_destroyed, last_i]
        {
            if (*d) return;

            _is_publishing = false;

            while (true) {
                bool is_last = last_i == _callbacks.begin();
                auto cb = move(_callbacks.front());
                _callbacks.pop_front();
                if (cb) cb();
                if (*d) return;
                if (is_last) break;
            }

            start_publishing();
        });
}

Republisher::~Republisher()
{
    *_was_destroyed = true;
}
