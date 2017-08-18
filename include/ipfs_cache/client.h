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

    void get_content(std::string url, std::function<void(std::string)>);

    ~Client();

private:
    void replay_queued_tasks();

private:
    std::unique_ptr<Db> _db;
    std::unique_ptr<Backend> _backend;
    std::queue<std::function<void()>> _queued_tasks;
};

} // ipfs_cache namespace

