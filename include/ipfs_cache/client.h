#pragma once

#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>
#include <functional>
#include <memory>
#include <string>
#include <json.hpp>

#include <ipfs_cache/cached_content.h>

namespace boost { namespace asio {
    class io_service;
}}

namespace ipfs_cache {

class Backend;
class ClientDb;
using Json = nlohmann::json;

class Client {
public:
    static std::unique_ptr<Client> build( boost::asio::io_service&
                                        , std::string ipns
                                        , std::string path_to_repo
                                        , std::function<void()>& cancel
                                        , boost::asio::yield_context);

    // This constructor may do repository initialization disk IO and as such
    // may block for a second or more. If that is undesirable, use the above
    // static async `build` function instead.
    Client(boost::asio::io_service&, std::string ipns, std::string path_to_repo);

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    Client(Client&&);
    Client& operator=(Client&&);

    std::string ipfs_add(const std::string& content, boost::asio::yield_context);

    // Find the content previously stored by the injector under `url`.
    // The content is returned in the parameter of the callback function.
    //
    // Basically it does this: Look into the database to find the IPFS_ID
    // correspoinding to the `url`, when found, fetch the content corresponding
    // to that IPFS_ID from IPFS.
    CachedContent get_content(std::string url, boost::asio::yield_context);

    void wait_for_db_update(boost::asio::yield_context);

    void set_ipns(std::string ipns);

    std::string id() const;

    const std::string& ipns() const;
    const std::string& ipfs() const;

    ~Client();

private:
    Client(Backend, std::string ipns, std::string path_to_repo);

private:
    std::string _path_to_repo;
    std::unique_ptr<Backend> _backend;
    std::unique_ptr<ClientDb> _db;
};

} // ipfs_cache namespace

