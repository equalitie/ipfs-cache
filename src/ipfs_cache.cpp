#include <assert.h>
#include <iostream>
#include <ipfs_cache/ipfs_cache.h>
#include <json.hpp>
#include <chrono>

#include "dispatch.h"
#include "backend.h"

using namespace std;
using namespace ipfs_cache;
using json = nlohmann::json;

// This class is simply so that we don't have to forward declare the json class
// in ipfs_cache.h (it has 9 template arguments with defaults).
struct ipfs_cache::Db : json { };

IpfsCache::IpfsCache(event_base* evbase)
    : _backend(new Backend(evbase, "./repo"))
{
    _backend->resolve(ipns_id(), [=](string ipfs_id) {
        _backend->get_content(ipfs_id, [=](string content) {
            try {
                _db.reset(new Db(json::parse(content)));
            }
            catch (std::exception e) {
                _db.reset(new Db());
            }

            replay_queued_tasks();
        });
    });
}

string IpfsCache::ipns_id() const
{
    return _backend->ipns_id();
}

void IpfsCache::update_db(string url, string ipfs_id, function<void()> cb)
{
    if (!_db) {
        return _queued_tasks.push([ url     = move(url)
                                  , ipfs_id = move(ipfs_id)
                                  , cb      = move(cb)]
                                  { update_db(url, ipfs_id, cb); });
    }

    (*_db)[url] = ipfs_id;

    string str = _db->dump();

    _backend->insert_content((uint8_t*) str.data(), str.size(),
        [=, cb = move(cb)](string db_ipfs_id) {
            _backend->publish(move(db_ipfs_id), [start, cb = move(cb)] {
                cb();
            });
        });
}

void IpfsCache::insert_content(const uint8_t* data, size_t size, function<void(string)> cb)
{
    return _backend->insert_content(data, size, move(cb));
}

void IpfsCache::get_content(const string& ipfs_id, function<void(string)> cb)
{
    return _backend->get_content(ipfs_id, move(cb));
}

void IpfsCache::replay_queued_tasks()
{
    auto tasks = move(_queued_tasks);

    while (!tasks.empty()) {
        auto task = move(tasks.front());
        tasks.pop();
        task();
    }
}

IpfsCache::~IpfsCache() {}
