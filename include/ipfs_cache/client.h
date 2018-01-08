#pragma once

#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>
#include <functional>
#include <memory>
#include <string>
#include <json.hpp>

namespace boost { namespace asio {
    class io_service;
}}

namespace ipfs_cache {

struct Backend;
struct Db;
using Json = nlohmann::json;

class Client {
public:
    Client(boost::asio::io_service&, std::string ipns, std::string path_to_repo);

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Find the content previously stored by the injector under `url`.
    // The content is returned in the parameter of the callback function.
    //
    // Basically it does this: Look into the database to find the IPFS_ID
    // correspoinding to the `url`, when found, fetch the content corresponding
    // to that IPFS_ID from IPFS.
    void get_content( std::string url
                    , std::function<void(boost::system::error_code, Json)>);

    Json get_content(std::string url, boost::asio::yield_context);

    const std::string& ipns() const;
    const std::string& ipfs() const;
    const Json& json_db() const;

    ~Client();

private:
    std::unique_ptr<Backend> _backend;
    std::unique_ptr<Db> _db;
};

} // ipfs_cache namespace

