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

        _backend->cat(ipfs_id, move(cb));
    }); 
}

Client::~Client() {}
