#pragma once

#include <string>
#include <functional>
#include <memory>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>

#include "namespaces.h"

namespace boost { namespace asio {
    class io_service;
}}

namespace ipfs_cache {

struct BackendImpl;

class Backend {
    using Timer = boost::asio::steady_timer;
    using Duration = Timer::duration;

    template<class Token, class... Ret>
    using Handler = typename asio::handler_type< Token
                                               , void(sys::error_code, Ret...)
                                               >::type;

    template<class Token, class... Ret>
    using Result = typename asio::async_result<Handler<Token, Ret...>>;

public:
    Backend(boost::asio::io_service&, const std::string& repo_path);

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    // Returns the IPNS CID of the database.
    std::string ipns_id() const;

    template<class Token>
    typename Result<Token, std::string>::type
    add(const uint8_t* data, size_t size, Token&&);

    template<class Token>
    typename Result<Token, std::string>::type
    add(const std::string&, Token&&); // Convenience function.

    template<class Token>
    typename Result<Token, std::string>::type
    cat(const std::string& cid, Duration timeout, Token&&);

    // The expected_size argument is being used to calculate
    // timeout value.
    template<class Token>
    typename Result<Token, std::string>::type
    cat(const std::string& cid, size_t expected_size, Token&&);

    template<class Token>
    void
    publish( const std::string& cid, Timer::duration, Token&&);

    template<class Token>
    typename Result<Token, std::string>::type
    resolve(const std::string& ipns_id, Token&&);

    boost::asio::io_service& get_io_service();

    ~Backend();

private:
    void add_( const uint8_t* data, size_t size
             , std::function<void(boost::system::error_code, std::string)>);

    void cat_( const std::string& cid
             , size_t expected_size
             , std::function<void(boost::system::error_code, std::string)>);

    void cat_( const std::string& cid
             , Duration timeout
             , std::function<void(boost::system::error_code, std::string)>);

    void publish_( const std::string& cid, Timer::duration
                 , std::function<void(boost::system::error_code)>);

    void resolve_( const std::string& ipns_id
                 , std::function<void(boost::system::error_code, std::string)>);

private:
    std::shared_ptr<BackendImpl> _impl;
};

template<class Token>
typename Backend::Result<Token, std::string>::type
Backend::add(const uint8_t* data, size_t size, Token&& token)
{
    Handler<Token, std::string> handler(std::forward<Token>(token));
    Result<Token, std::string> result(handler);
    add_(data, size, std::move(handler));
    return result.get();
}

template<class Token>
typename Backend::Result<Token, std::string>::type
Backend::add(const std::string& data, Token&& token)
{
    Handler<Token, std::string> handler(std::forward<Token>(token));
    Result<Token, std::string> result(handler);
    add_( reinterpret_cast<const uint8_t*>(data.c_str())
        , data.size()
        , std::move(handler));
    return result.get();
}

template<class Token>
typename Backend::Result<Token, std::string>::type
Backend::cat(const std::string& cid, size_t expected_size, Token&& token)
{
    Handler<Token, std::string> handler(std::forward<Token>(token));
    Result<Token, std::string> result(handler);
    cat_(cid, expected_size, std::move(handler));
    return result.get();
}

template<class Token>
typename Backend::Result<Token, std::string>::type
Backend::cat(const std::string& cid, Duration timeout, Token&& token)
{
    Handler<Token, std::string> handler(std::forward<Token>(token));
    Result<Token, std::string> result(handler);
    cat_(cid, timeout, std::move(handler));
    return result.get();
}

template<class Token>
void
Backend::publish(const std::string& cid, Timer::duration d, Token&& token)
{
    Handler<Token> handler(std::forward<Token>(token));
    Result<Token> result(handler);
    publish_(cid, d, std::move(handler));
    return result.get();
}

template<class Token>
typename Backend::Result<Token, std::string>::type
Backend::resolve(const std::string& ipns_id, Token&& token)
{
    Handler<Token, std::string> handler(std::forward<Token>(token));
    Result<Token, std::string> result(handler);
    resolve_(ipns_id, std::move(handler));
    return result.get();
}

} // ipfs_cache namespace
