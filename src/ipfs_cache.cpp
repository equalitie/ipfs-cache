#include <assert.h>
#include <iostream>
#include <ipfs_cache/ipfs_cache.h>
#include <json.hpp>

#include "dispatch.h"
#include "backend.h"

using namespace std;
using namespace ipfs_cache;
using json = nlohmann::json;

// This class is simply so that we don't have to forward declare the json class
// in ipfs_cache.h (it has 9 template arguments with defaults).
struct ipfs_cache::Db : json { };

IpfsCache::IpfsCache(event_base* evbase)
    : _db(new Db)
    , _backend(new Backend(evbase))
{
}

string IpfsCache::ipns_id() const
{
    return _backend->ipns_id();
}

void IpfsCache::update_db(string url, string cid, std::function<void()> cb)
{
    (*_db)[url] = cid;

    string str = _db->dump();

    _backend->insert_content((uint8_t*) str.data(), str.size(),
        [=, cb = move(cb)](string cid){
            _backend->publish(move(cid), [cb = move(cb)]() {
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

IpfsCache::~IpfsCache() {}
