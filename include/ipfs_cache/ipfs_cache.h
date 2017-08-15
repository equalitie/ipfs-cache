#pragma once

#include <event.h>
#include <functional>
#include <memory>

#include <ipfs_cache/query.h>

namespace ipfs_cache {

struct IpfsCacheImpl;

class IpfsCache {
public:
    IpfsCache(event_base*);

    IpfsCache(const IpfsCache&) = delete;
    IpfsCache& operator=(const IpfsCache&) = delete;

    // Returns the IPNS CID of the database.
    std::string ipns_id() const;

    void update_db(const entry& query, std::function<void(std::string)> callback);

    ~IpfsCache();

private:
    std::shared_ptr<IpfsCacheImpl> _impl;
};

} // ipfs_cache namespace
