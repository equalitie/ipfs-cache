#pragma once

#include <event2/event.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <ipfs_cache/timer.h>


namespace ipfs_cache {

struct BackendImpl;

class Backend {
public:
    Backend(event_base*, const std::string& repo_path);

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    // Returns the IPNS CID of the database.
    std::string ipns_id() const;

    void add(const uint8_t* data, size_t size, std::function<void(std::string)>);
    void add(const std::vector<char>&, std::function<void(std::string)>); // Convenience function.

    void cat(const std::string& cid, std::function<void(std::vector<char>)>);
    void publish(const std::string& cid, Timer::Duration, std::function<void()>);
    void resolve(const std::string& ipns_id, std::function<void(std::string)>);

    event_base* evbase() const;

    ~Backend();

private:
    std::shared_ptr<BackendImpl> _impl;
};

} // ipfs_cache namespace
