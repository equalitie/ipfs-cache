#include <go_ipfs_cache.h>

#include "backend.h"

using namespace ipfs_cache;
using namespace std;

struct ipfs_cache::BackendImpl {
    bool was_destroyed;
    event_base* evbase;

    BackendImpl(event_base* evbase)
        : was_destroyed(false)
        , evbase(evbase)
    {}
};

Backend::Backend(event_base* evbase)
    : _impl(make_shared<BackendImpl>(evbase))
{
    bool started = go_ipfs_cache_start();

    if (!started) {
        throw std::runtime_error("Backend: Failed to start IPFS");
    }
}

string Backend::ipns_id() const {
    char* cid = go_ipfs_cache_ipns_id();
    string ret(cid);
    free(cid);
    return ret;
}

void Backend::publish(const string& cid, std::function<void()> cb)
{
    struct OnPublish {
        shared_ptr<BackendImpl> impl;
        function<void()> cb;

        static void on_publish(void* arg) {
            auto self = reinterpret_cast<OnPublish*>(arg);
            if (self->impl->was_destroyed) return;
            auto cb   = move(self->cb);
            auto impl = move(self->impl);
            delete self;
            cb();
        }
    };

    go_ipfs_cache_publish( (char*) cid.data()
                         , (void*) OnPublish::on_publish
                         , (void*) new OnPublish{_impl, move(cb)});
}

void Backend::insert_content(const uint8_t* data, size_t size, function<void(string)> cb)
{
    struct OnInsert {
        shared_ptr<BackendImpl> impl;
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

Backend::~Backend()
{
    _impl->was_destroyed = true;
    go_ipfs_cache_stop();
}
