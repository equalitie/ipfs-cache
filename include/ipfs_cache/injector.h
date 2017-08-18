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

    void update_db(std::string url, std::string ipfs_id, std::function<void()>);
    void query_db(std::string url, std::function<void(std::string)>);

    void insert_content(const uint8_t* data, size_t size, std::function<void(std::string)>);
    void get_content(const std::string& ipfs_id, std::function<void(std::string)>);

    ~Injector();

private:
    void replay_queued_tasks();

private:
    std::unique_ptr<Db> _db;
    std::unique_ptr<Backend> _backend;
    std::queue<std::function<void()>> _queued_tasks;
};

} // ipfs_cache namespace

