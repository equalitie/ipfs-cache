#include <ipfs_bindings.h>
#include <assert.h>
#include <mutex>

#include "backend.h"
#include "dispatch.h"

using namespace ipfs_cache;
using namespace std;

namespace {
    const uint32_t CID_SIZE = 46;
}

struct ipfs_cache::BackendImpl {
    // This prevents callbacks from being called once Backend is destroyed.
    bool was_destroyed;
    event_base* evbase;
    mutex destruct_mutex;

    BackendImpl(event_base* evbase)
        : was_destroyed(false)
        , evbase(evbase)
    {}
};

struct HandleVoid {
    shared_ptr<BackendImpl> impl;
    function<void()> cb;

    static void call(void* arg) {
        auto self = reinterpret_cast<HandleVoid*>(arg);
        auto cb   = move(self->cb);
        auto impl = move(self->impl);
        auto evb  = impl->evbase;

        delete self;

        lock_guard<mutex> guard(impl->destruct_mutex);

        if (impl->was_destroyed) return;

        dispatch(evb, [cb = move(cb), impl = move(impl)]() {
            if (impl->was_destroyed) return;
            cb();
        });
    }
};

template<class D>
struct HandleData {
    shared_ptr<BackendImpl> impl;
    function<void(D)> cb;

    static void call(const char* data, size_t size, void* arg) {
        auto self = reinterpret_cast<HandleData*>(arg);
        auto cb   = move(self->cb);
        auto impl = move(self->impl);
        auto evb  = impl->evbase;

        delete self;

        lock_guard<mutex> guard(impl->destruct_mutex);

        if (impl->was_destroyed) return;

        dispatch(evb, [ cb   = move(cb)
                      , d    = D(data, data + size)
                      , impl = move(impl)]() {
            if (impl->was_destroyed) return;
            cb(move(d));
        });
    }
};

Backend::Backend(event_base* evbase, const string& repo_path)
    : _impl(make_shared<BackendImpl>(evbase))
{
    bool started = go_ipfs_cache_start((char*) repo_path.data());

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

void Backend::publish(const string& cid, Timer::Duration d, std::function<void()> cb)
{
    using namespace std::chrono;

    assert(cid.size() == CID_SIZE);

    go_ipfs_cache_publish( (char*) cid.data()
                         , duration_cast<seconds>(d).count()
                         , (void*) HandleVoid::call
                         , (void*) new HandleVoid{_impl, move(cb)});
}

void Backend::resolve(const string& ipns_id, function<void(string)> cb)
{
    go_ipfs_cache_resolve( (char*) ipns_id.data()
                         , (void*) HandleData<string>::call
                         , (void*) new HandleData<string>{_impl, move(cb)} );
}

void Backend::add(const uint8_t* data, size_t size, function<void(string)> cb)
{
    go_ipfs_cache_add( (void*) data, size
                     , (void*) HandleData<string>::call
                     , (void*) new HandleData<string>{_impl, move(cb)} );
}

void Backend::add(const vector<char>& s, function<void(string)> cb)
{
    add((const uint8_t*) s.data(), s.size(), move(cb));
}

void Backend::cat(const string& ipfs_id, function<void(vector<char>)> cb)
{
    assert(ipfs_id.size() == CID_SIZE);

    go_ipfs_cache_cat( (char*) ipfs_id.data()
                     , (void*) HandleData<vector<char>>::call
                     , (void*) new HandleData<vector<char>>{_impl, move(cb)} );
}

void Backend::test_put()
{
    go_ipfs_cache_put_value();
}

void Backend::test_get()
{
    go_ipfs_cache_get_value();
}

event_base* Backend::evbase() const
{
    return _impl->evbase;
}

Backend::~Backend()
{
    lock_guard<mutex> guard(_impl->destruct_mutex);
    _impl->was_destroyed = true;
    go_ipfs_cache_stop();
}
