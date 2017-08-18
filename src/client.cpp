#include <assert.h>
#include <iostream>
#include <chrono>

#include <ipfs_cache/client.h>

#include "dispatch.h"
#include "backend.h"
#include "db.h"

using namespace std;
using namespace ipfs_cache;

Client::Client(event_base* evbase, string ipns, string path_to_repo)
    : _backend(new Backend(evbase, path_to_repo))
    , _db(new Db(*_backend, ipns))
{
}

void Client::get_content(string url, std::function<void(string)> cb)
{
    _db->query(url, [this, cb = move(cb)](string ipfs_id) {
         if (ipfs_id.empty()) {
            return cb(move(ipfs_id));
         }

        _backend->cat(ipfs_id, [cb = move(cb)](string content) {
            cb(content);
        });
    }); 
}

void Client::replay_queued_tasks()
{
    auto tasks = move(_queued_tasks);

    while (!tasks.empty()) {
        auto task = move(tasks.front());
        tasks.pop();
        task();
    }
}

Client::~Client() {}
