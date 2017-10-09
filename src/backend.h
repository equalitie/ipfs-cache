#pragma once

#include <string>
#include <functional>
#include <memory>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

namespace boost { namespace asio {
    class io_service;
}}

namespace ipfs_cache {

struct BackendImpl;

class Backend {
    using Timer = boost::asio::steady_timer;

public:
    Backend(boost::asio::io_service&, const std::string& repo_path);

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    // Returns the IPNS CID of the database.
    std::string ipns_id() const;

    void add( const uint8_t* data, size_t size
            , std::function<void(boost::system::error_code, std::string)>);
    void add( const std::string&
            , std::function<void(boost::system::error_code, std::string)>); // Convenience function.

    void cat( const std::string& cid
            , std::function<void(boost::system::error_code, std::string)>);
    void publish( const std::string& cid, Timer::duration
                , std::function<void(boost::system::error_code)>);
    void resolve( const std::string& ipns_id
                , std::function<void(boost::system::error_code, std::string)>);

    boost::asio::io_service& get_io_service();

    ~Backend();

private:
    std::shared_ptr<BackendImpl> _impl;
};

} // ipfs_cache namespace
