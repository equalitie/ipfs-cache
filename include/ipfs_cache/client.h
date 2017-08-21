#pragma once

#include <event2/event.h>
#include <functional>
#include <memory>
#include <queue>

namespace ipfs_cache {

struct Backend;
struct Db;

class Client {
public:
    Client(event_base*, std::string ipns, std::string path_to_repo);

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Find the content previously stored by the injector under `url`.
    // The content is returned in the parameter of the callback function.
    //
    // Basically it does this: Look into the database to find the IPFS_ID
    // correspoinding to the `url`, when found, fetch the content corresponding
    // to that IPFS_ID from IPFS.
    void get_content(std::string url, std::function<void(std::string)>);

    ~Client();

private:
    std::unique_ptr<Backend> _backend;
    std::unique_ptr<Db> _db;
    std::queue<std::function<void()>> _queued_tasks;
};

} // ipfs_cache namespace

