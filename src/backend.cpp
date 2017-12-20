#include <ipfs_bindings.h>
#include <ipfs_cache/error.h>
#include <assert.h>
#include <mutex>
#include <experimental/tuple>

#include "backend.h"

using namespace ipfs_cache;
using namespace std;

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

template<class F> struct Defer {
    F f;
    ~Defer() { f(); }
};

template<class F> Defer<F> defer(F&& f) {
    return Defer<F>{forward<F>(f)};
};

template<class... As>
struct Handle {
    shared_ptr<BackendImpl> impl;
    function<void(sys::error_code, As&&...)> cb;
    tuple<sys::error_code, As...> args;

    static void call(int err, void* arg, As... args) {
        auto self = reinterpret_cast<Handle*>(arg);
        auto& ios = self->impl->ios;

        lock_guard<mutex> guard(self->impl->destruct_mutex);

        if (self->impl->was_destroyed) return;

        auto ec = make_error_code(error::ipfs_error{err});
        self->args = make_tuple(ec, move(args)...);

        ios.dispatch([self] {
            auto on_exit = defer([=] { delete self; });
            if (self->impl->was_destroyed) return;
            std::experimental::apply(self->cb, move(self->args));
        });
    }

    static void call_void(int err, void* arg) {
        call(err, arg);
    }

    static void call_data(int err, const char* data, size_t size, void* arg) {
        call(err, arg, string(data, data + size));
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

void Backend::publish_(const string& cid, Timer::duration d, std::function<void(sys::error_code)> cb)
{
    using namespace std::chrono;

    assert(cid.size() == CID_SIZE);

    go_ipfs_cache_publish( (char*) cid.data()
                         , duration_cast<seconds>(d).count()
                         , (void*) Handle<>::call_void
                         , (void*) new Handle<>{_impl, move(cb)});
}

void Backend::resolve_(const string& ipns_id, function<void(sys::error_code, string)> cb)
{
    go_ipfs_cache_resolve( (char*) ipns_id.data()
                         , (void*) Handle<string>::call_data
                         , (void*) new Handle<string>{_impl, move(cb)} );
}

void Backend::add_(const uint8_t* data, size_t size, function<void(sys::error_code, string)> cb)
{
    go_ipfs_cache_add( (void*) data, size
                     , (void*) Handle<string>::call_data
                     , (void*) new Handle<string>{_impl, move(cb)} );
}

void Backend::cat_(const string& ipfs_id, function<void(sys::error_code, string)> cb)
{
    assert(ipfs_id.size() == CID_SIZE);

    go_ipfs_cache_cat( (char*) ipfs_id.data()
                     , (void*) Handle<string>::call_data
                     , (void*) new Handle<string>{_impl, move(cb)} );
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
