#include <assert.h>
#include <iostream>
#include <ipfs_cache/ipfs_cache.h>
#include <go_ipfs_cache.h>

#include "dispatch.h"
#include "query_view.h"

using namespace std;
using namespace ipfs_cache;

struct ipfs_cache::IpfsCacheImpl {
    bool was_destroyed;
    event_base* evbase;

    IpfsCacheImpl(event_base* evbase)
        : was_destroyed(false)
        , evbase(evbase)
    {}
};

IpfsCache::IpfsCache(event_base* evbase)
    : _impl(make_shared<IpfsCacheImpl>(evbase))
{
    bool started = go_ipfs_cache_start();

    if (!started) {
        throw std::runtime_error("Failed to start IPFS");
    }
}

string IpfsCache::ipns_id() const {
    char* cid = go_ipfs_cache_ipns_id();
    string ret(cid);
    free(cid);
    return ret;
}

void IpfsCache::update_db(const entry& e, std::function<void(std::string)> callback)
{
    struct OnAdd {
        shared_ptr<IpfsCacheImpl> impl;
        function<void(string)> cb;

        static void on_add(const char* data, size_t size, void* arg) {
            auto self = reinterpret_cast<OnAdd*>(arg);
            if (self->impl->was_destroyed) return;
            auto cb   = move(self->cb);
            auto impl = move(self->impl);
            delete self;
            cb(string(data, data + size));
        }
    };

    with_query_view(e, [&](query_view* dv) {
        go_ipfs_cache_update_db( dv
                               , (void*) OnAdd::on_add
                               , (void*) new OnAdd{_impl, move(callback)});
    });
}

void IpfsCache::insert_content(const uint8_t* data, size_t size, function<void(string)> cb) {
    struct OnInsert {
        shared_ptr<IpfsCacheImpl> impl;
        function<void(string)> cb;

        static void on_insert(const char* data, size_t size, void* arg) {
            auto self = reinterpret_cast<OnInsert*>(arg);
            if (self->impl->was_destroyed) return;
            auto cb = move(self->cb);
            auto impl = move(self->impl);
            delete self;
            cb(string(data, data + size)); // TODO: string_view?
        }
    };

    go_ipfs_cache_insert_content( (void*) data, size
                                , (void*) OnInsert::on_insert
                                , (void*) new OnInsert{_impl, move(cb)} );
}

IpfsCache::~IpfsCache()
{
    _impl->was_destroyed = true;
}
