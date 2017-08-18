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
{
    _backend->resolve(ipns, [=](string ipfs_id) {
        if (ipfs_id.size() == 0) {
            _db.reset(new Db());
            replay_queued_tasks();
            return;
        }

        _backend->get_content(ipfs_id, [=](string content) {
            try {
                _db.reset(new Db(content));
            }
            catch (const std::exception& e) {
                _db.reset(new Db());
            }

            replay_queued_tasks();
        });
    });
}

void Client::get_content(string url, std::function<void(string)> cb)
{
    if (!_db) {
        return _queued_tasks.push([ url = move(url)
                                  , cb  = move(cb)
                                  , this ]
                                  { get_content(move(url), move(cb)); });
    }

    auto value = _db->json[url];

    if (!value.is_string()) {
        dispatch(_backend->evbase(), [c = move(cb)] { c(""); });
        return;
    }

    string ipfs_id = value;

    _backend->get_content(ipfs_id,
        [cb = move(cb), this](string content) {
            cb(move(content));
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
