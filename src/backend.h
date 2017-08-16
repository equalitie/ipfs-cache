#pragma once

#include <event.h>
#include <string>
#include <functional>
#include <memory>

namespace ipfs_cache {

struct BackendImpl;

class Backend {
public:
    Backend(event_base*);

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    // Returns the IPNS CID of the database.
    std::string ipns_id() const;

    void publish(const std::string& cid, std::function<void()>);
    void insert_content(const uint8_t* data, size_t size, std::function<void(std::string)>);

    ~Backend();

private:
    std::shared_ptr<BackendImpl> _impl;
};

} // ipfs_cache namespace
