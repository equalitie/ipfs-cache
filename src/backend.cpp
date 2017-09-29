#include <ipfs_bindings.h>
#include <ipfs_cache/error.h>
#include <assert.h>
#include <mutex>

#include "backend.h"

using namespace ipfs_cache;
using namespace std;
namespace asio = boost::asio;
namespace sys  = boost::system;

namespace {
    const uint32_t CID_SIZE = 46;
}

struct ipfs_cache::BackendImpl {
    // This prevents callbacks from being called once Backend is destroyed.
    bool was_destroyed;
    asio::io_service& ios;
    mutex destruct_mutex;

    BackendImpl(asio::io_service& ios)
        : was_destroyed(false)
        , ios(ios)
    {}
};

struct HandleVoid {
    shared_ptr<BackendImpl> impl;
    function<void(sys::error_code)> cb;

    static void call(int err, void* arg) {
        auto self = reinterpret_cast<HandleVoid*>(arg);
        auto cb   = move(self->cb);
        auto e    = error::ipfs_error{err};
        auto impl = move(self->impl);
        auto& ios = impl->ios;

        delete self;

        lock_guard<mutex> guard(impl->destruct_mutex);

        if (impl->was_destroyed) return;

        ios.dispatch( [ cb   = move(cb)
                      , ec   = error::make_error_code(e)
                      , impl = move(impl)]() {
            if (impl->was_destroyed) return;
            cb(ec);
        });
    }
};

struct HandleData {
    shared_ptr<BackendImpl> impl;
    function<void(sys::error_code, string)> cb;

    static void call(int err, const char* data, size_t size, void* arg) {
        auto self = reinterpret_cast<HandleData*>(arg);
        auto cb   = move(self->cb);
        auto e    = error::ipfs_error{err};
        auto impl = move(self->impl);
        auto& ios = impl->ios;

        delete self;

        lock_guard<mutex> guard(impl->destruct_mutex);

        if (impl->was_destroyed) return;

        ios.dispatch( [ cb   = move(cb)
                      , ec   = error::make_error_code(e)
                      , s    = string(data, data + size)
                      , impl = move(impl)]() {
            if (impl->was_destroyed) return;
            cb(ec, move(s));
        });
    }
};

Backend::Backend(asio::io_service& ios, const string& repo_path)
    : _impl(make_shared<BackendImpl>(ios))
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

void Backend::publish(const string& cid, Timer::duration d, std::function<void(sys::error_code)> cb)
{
    using namespace std::chrono;

    assert(cid.size() == CID_SIZE);

    go_ipfs_cache_publish( (char*) cid.data()
                         , duration_cast<seconds>(d).count()
                         , (void*) HandleVoid::call
                         , (void*) new HandleVoid{_impl, move(cb)});
}

void Backend::resolve(const string& ipns_id, function<void(sys::error_code, string)> cb)
{
    go_ipfs_cache_resolve( (char*) ipns_id.data()
                         , (void*) HandleData::call
                         , (void*) new HandleData{_impl, move(cb)} );
}

void Backend::add(const uint8_t* data, size_t size, function<void(sys::error_code, string)> cb)
{
    go_ipfs_cache_add( (void*) data, size
                     , (void*) HandleData::call
                     , (void*) new HandleData{_impl, move(cb)} );
}

void Backend::add(const string& s, function<void(sys::error_code, string)> cb)
{
    add((const uint8_t*) s.data(), s.size(), move(cb));
}

void Backend::cat(const string& ipfs_id, function<void(sys::error_code, string)> cb)
{
    assert(ipfs_id.size() == CID_SIZE);

    go_ipfs_cache_cat( (char*) ipfs_id.data()
                     , (void*) HandleData::call
                     , (void*) new HandleData{_impl, move(cb)} );
}

boost::asio::io_service& Backend::get_io_service()
{
    return _impl->ios;
}

Backend::~Backend()
{
    lock_guard<mutex> guard(_impl->destruct_mutex);
    _impl->was_destroyed = true;
    go_ipfs_cache_stop();
}
