#pragma once

#include <event2/event.h>
#include <functional>
#include <memory>
#include <queue>

namespace ipfs_cache {

struct Backend;
struct Db;

class Injector {
public:
    Injector(event_base*, std::string path_to_repo);

    Injector(const Injector&) = delete;
    Injector& operator=(const Injector&) = delete;

    // Returns the IPNS CID of the database.
    // The database could be then looked up by e.g. pointing your browser to:
    // "https://ipfs.io/ipns/" + ipfs.ipns_id()
    std::string ipns_id() const;

    void insert_content( std::string url
                       , const std::string& content
                       , std::function<void(std::string)>);

    ~Injector();

private:
    std::unique_ptr<Backend> _backend;
    std::unique_ptr<Db> _db;
};

} // ipfs_cache namespace

