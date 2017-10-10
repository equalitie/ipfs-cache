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
    using OnInsert = std::function<void(boost::system::error_code, std::string)>;

private:
    struct InsertEntry {
        std::string key;
        std::string value;
        OnInsert    on_insert;
    };

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
                       , OnInsert);

    std::string insert_content( std::string url
                              , const std::string& content
                              , boost::asio::yield_context);

    ~Injector();

private:
    void insert_content_from_queue();

private:
    std::unique_ptr<Backend> _backend;
    std::unique_ptr<Db> _db;
    std::queue<InsertEntry> _insert_queue;
    const unsigned int _concurrency = 8;
    unsigned int _job_count = 0;
};

} // ipfs_cache namespace

