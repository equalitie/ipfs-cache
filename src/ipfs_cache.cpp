#include <assert.h>
#include <iostream>
#include <ipfs_cache/ipfs_cache.h>
#include <go_ipfs_cache.h>

#include "dispatch.h"
#include "data_view.h"

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
    go_ipfs_cache_start();
}

void IpfsCache::update_db(const entry& e, std::function<void(std::string)> callback)
{
    struct OnAdd {
        shared_ptr<IpfsCacheImpl> impl;
        function<void(string)> cb;

        static void on_add(const char* data, size_t size, void* arg) {
            auto self = reinterpret_cast<OnAdd*>(arg);
            if (self->impl->was_destroyed) return;
            auto cb = move(self->cb);
            cb(string(data, data + size));
            delete self;
        }
    };

    with_data_view(e, [&](data_view* dv) {
        go_ipfs_cache_update_db( dv
                               , (void*) OnAdd::on_add
                               , (void*) new OnAdd{_impl, move(callback)});
    });
}

IpfsCache::~IpfsCache()
{
    _impl->was_destroyed = true;
}
