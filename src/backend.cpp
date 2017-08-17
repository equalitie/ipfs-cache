#include <go_ipfs_cache.h>
#include <iostream>

#include "backend.h"
#include "dispatch.h"

using namespace ipfs_cache;
using namespace std;

struct ipfs_cache::BackendImpl {
    // This prevents callbacks from being called once Backend is destroyed.
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
    string repo("./repo");

    bool started = go_ipfs_cache_start((char*) repo.data());

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

        static void call(void* arg) {
            auto self = reinterpret_cast<OnPublish*>(arg);
            auto cb   = move(self->cb);
            auto impl = move(self->impl);
            auto evb  = impl->evbase;

            delete self;

            dispatch(evb, [cb = move(cb), impl = move(impl)]() {
                if (impl->was_destroyed) return;
                cb();
            });
        }
    };

    go_ipfs_cache_publish( (char*) cid.data()
                         , (void*) OnPublish::call
                         , (void*) new OnPublish{_impl, move(cb)});
}

void Backend::insert_content(const uint8_t* data, size_t size, function<void(string)> cb)
{
    struct OnInsert {
        shared_ptr<BackendImpl> impl;
        function<void(string)> cb;

        static void call(const char* data, size_t size, void* arg) {
            auto self = reinterpret_cast<OnInsert*>(arg);
            auto cb   = move(self->cb);
            auto impl = move(self->impl);
            auto evb  = impl->evbase;

            delete self;

            dispatch(evb, [ cb   = move(cb)
                          , s    = string(data, data + size)
                          , impl = move(impl)]() {
                if (impl->was_destroyed) return;
                cb(move(s));
            });
        }
    };

    go_ipfs_cache_insert_content( (void*) data, size
                                , (void*) OnInsert::call
                                , (void*) new OnInsert{_impl, move(cb)} );
}

void Backend::get_content(const std::string& ipfs_id, function<void(string)> cb)
{
    struct OnGet {
        shared_ptr<BackendImpl> impl;
        function<void(string)> cb;

        static void call(const char* data, size_t size, void* arg) {
            auto self = reinterpret_cast<OnGet*>(arg);
            auto cb   = move(self->cb);
            auto impl = move(self->impl);
            auto evb  = impl->evbase;

            delete self;

            dispatch(evb, [ cb   = move(cb)
                          , s    = string(data, data + size)
                          , impl = move(impl)]() {
                if (impl->was_destroyed) return;
                cb(move(s));
            });
        }
    };

    go_ipfs_cache_get_content( (char*) ipfs_id.data()
                             , (void*) OnGet::call
                             , (void*) new OnGet{_impl, move(cb)} );
}

Backend::~Backend()
{
    _impl->was_destroyed = true;
    go_ipfs_cache_stop();
}
