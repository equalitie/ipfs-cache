#pragma once

#include <event.h>
#include <functional>
#include <memory>

namespace ipfs_cache {

struct Backend;
struct Db;

class IpfsCache {
public:
    IpfsCache(event_base*);

    IpfsCache(const IpfsCache&) = delete;
    IpfsCache& operator=(const IpfsCache&) = delete;

    // Returns the IPNS CID of the database.
    // The database could be then looked up by e.g. pointing your browser to:
    // "https://ipfs.io/ipns/" + ipfs.ipns_id()
    std::string ipns_id() const;

    void update_db(std::string url, std::string cid, std::function<void()>);

    void insert_content(const uint8_t* data, size_t size, std::function<void(std::string)>);

    ~IpfsCache();

private:
    std::unique_ptr<Db> _db;
    std::unique_ptr<Backend> _backend;
};

} // ipfs_cache namespace
