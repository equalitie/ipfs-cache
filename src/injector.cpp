#include <assert.h>
#include <iostream>
#include <chrono>

#include <ipfs_cache/injector.h>

#include "dispatch.h"
#include "backend.h"
#include "db.h"

using namespace std;
using namespace ipfs_cache;

Injector::Injector(event_base* evbase, string path_to_repo)
    : _backend(new Backend(evbase, path_to_repo))
{
    auto ipns = _backend->ipns_id();

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

string Injector::ipns_id() const
{
    return _backend->ipns_id();
}

void Injector::update_db(string url, string ipfs_id, function<void()> cb)
{
    if (!_db) {
        return _queued_tasks.push(
                [ url     = move(url)
                , ipfs_id = move(ipfs_id)
                , cb      = move(cb)
                , this ]
                { update_db(move(url), move(ipfs_id), move(cb)); });
    }

    try {
        _db->json[url] = ipfs_id;
        string str = _db->json.dump();

        _backend->insert_content((uint8_t*) str.data(), str.size(),
            [=, cb = move(cb)](string db_ipfs_id) {
                _backend->publish(move(db_ipfs_id), [cb = move(cb)] {
                    cb();
                });
            });
    }
    catch(...) {
        assert(0);
    }
}

void Injector::query_db(string url, std::function<void(string)> cb)
{
    if (!_db) {
        return _queued_tasks.push([ url = move(url)
                                  , cb  = move(cb)
                                  , this ]
                                  { query_db(move(url), move(cb)); });
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

void Injector::insert_content(const uint8_t* data, size_t size, function<void(string)> cb)
{
    return _backend->insert_content(data, size, move(cb));
}

void Injector::get_content(const string& ipfs_id, function<void(string)> cb)
{
    return _backend->get_content(ipfs_id, move(cb));
}

void Injector::replay_queued_tasks()
{
    auto tasks = move(_queued_tasks);

    while (!tasks.empty()) {
        auto task = move(tasks.front());
        tasks.pop();
        task();
    }
}

Injector::~Injector() {}
