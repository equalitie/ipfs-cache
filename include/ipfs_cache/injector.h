#pragma once

#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>
#include <functional>
#include <memory>
#include <queue>

namespace boost { namespace asio {
    class io_service;
}}

namespace ipfs_cache {

struct Backend;
struct Db;

class Injector {
public:
    Injector(boost::asio::io_service&, std::string path_to_repo);

    Injector(const Injector&) = delete;
    Injector& operator=(const Injector&) = delete;

    // Returns the IPNS CID of the database.
    // The database could be then looked up by e.g. pointing your browser to:
    // "https://ipfs.io/ipns/" + ipfs.ipns_id()
    std::string ipns_id() const;

    // Insert `content` into IPFS and store its IPFS ID under the `url` in the
    // database. The IPFS ID is also returned as a parameter to the callback
    // function.
    //
    // When testing or debugging, the content can be found here:
    // "https://ipfs.io/ipfs/" + <IPFS ID>
    void insert_content( std::string url
                       , const std::string& content
                       , std::function<void(boost::system::error_code, std::string)>);

    std::string insert_content( std::string url
                              , const std::string& content
                              , boost::asio::yield_context);

    ~Injector();

private:
    std::unique_ptr<Backend> _backend;
    std::unique_ptr<Db> _db;
};

} // ipfs_cache namespace

