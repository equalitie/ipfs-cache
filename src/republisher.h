#pragma once

#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <string>
#include <memory>
#include <list>

namespace ipfs_cache {

class Backend;

/*
 * When a value is published into the network it is stored onto some nodes with
 * an expiration time. Additionaly, nodes on the network come ang go, and thus
 * the value needs to be periodically re-published.
 *
 * This class periodically republished last value used in the
 * Republisher::publish function.
 */
class Republisher {
public:
    Republisher(Backend&);

    void publish( const std::string&
                , std::function<void(boost::system::error_code)>);

    ~Republisher();

private:
    void start_publishing();

private:
    std::shared_ptr<bool> _was_destroyed;
    Backend& _backend;
    boost::asio::steady_timer _timer;
    bool _is_publishing = false;
    std::string _to_publish;
    std::list<std::function<void(boost::system::error_code)>> _callbacks;
};

}
