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

    template<class Token, class... Ret>
    using Handler = typename asio::handler_type< Token
                                               , void(sys::error_code, Ret...)
                                               >::type;

    template<class Token, class... Ret>
    using Result = typename asio::async_result<Handler<Token, Ret...>>;

public:
    // This constructor may do repository initialization disk IO and as such
    // may block for a second or more. If that is undesired, use the static
    // async `Backend::build` function instead.
    Backend(boost::asio::io_service&, const std::string& repo_path);

    Backend(Backend&&) = default;
    Backend& operator=(Backend&&) = default;

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    template<class Token>
    static
    typename Result<Token, std::unique_ptr<Backend>>::type
    build(boost::asio::io_service&, const std::string& repo_path, Token&&);

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
    cat(const std::string& cid, Token&&);

    template<class Token>
    void
    publish( const std::string& cid, Timer::duration, Token&&);

    template<class Token>
    typename Result<Token, std::string>::type
    resolve(const std::string& ipns_id, Token&&);

    boost::asio::io_service& get_io_service();

    ~Backend();

private:
    Backend(std::shared_ptr<BackendImpl>);

private:
    static
    void build_( boost::asio::io_service& ios
               , const std::string& repo_path
               , std::function<void( const boost::system::error_code&
                                   , std::unique_ptr<Backend>)>);

    void add_( const uint8_t* data, size_t size
             , std::function<void(boost::system::error_code, std::string)>);

    void cat_( const std::string& cid
             , std::function<void(boost::system::error_code, std::string)>);

    void publish_( const std::string& cid, Timer::duration
                 , std::function<void(boost::system::error_code)>);

    void resolve_( const std::string& ipns_id
                 , std::function<void(boost::system::error_code, std::string)>);

private:
    std::shared_ptr<BackendImpl> _impl;
};

template<class Token>
typename Backend::Result<Token, std::unique_ptr<Backend>>::type
Backend::build( boost::asio::io_service& ios
              , const std::string& repo_path
              , Token&& token)
{
    using BackendP = std::unique_ptr<Backend>;
    Handler<Token, BackendP> handler(std::forward<Token>(token));
    Result<Token, BackendP> result(handler);
    build_(ios, repo_path, std::move(handler));
    return result.get();
}

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
Backend::cat(const std::string& cid, Token&& token)
{
    Handler<Token, std::string> handler(std::forward<Token>(token));
    Result<Token, std::string> result(handler);
    cat_(cid, std::move(handler));
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
