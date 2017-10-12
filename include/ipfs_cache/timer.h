#pragma once

#include <boost/asio/io_service.hpp>
#include <chrono>
#include <functional>

namespace ipfs_cache {

class Timer {
private:
    struct Impl;

public:
    using Clock = std::chrono::steady_clock;

public:
    Timer(boost::asio::io_service& ios);

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    Timer(Timer&&) = default;
    Timer& operator=(Timer&&) = default;

    void start(Clock::duration, std::function<void()>);

    bool is_running() const;

    void stop();

    ~Timer();

private:
    boost::asio::io_service& _ios;
    std::shared_ptr<Impl> _impl;
};

} // ipfs_cache namespace
