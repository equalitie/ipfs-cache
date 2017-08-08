#pragma once

#include <event.h>
#include <functional>
#include <memory>

#include <ipfs_cache/data.h>

namespace ipfs_cache {

struct IpfsCacheImpl;

class IpfsCache {
public:
    IpfsCache(event_base*);

    IpfsCache(const IpfsCache&) = delete;
    IpfsCache& operator=(const IpfsCache&) = delete;

    void update_db(const entry&, std::function<void(std::string)> callback);

    ~IpfsCache();

private:
    std::shared_ptr<IpfsCacheImpl> _impl;
};

} // ipfs_cache namespace
