#include <boost/asio/io_service.hpp>
#include <assert.h>
#include <iostream>
#include <chrono>

#include <ipfs_cache/injector.h>

#include "backend.h"
#include "db.h"

using namespace std;
using namespace ipfs_cache;

namespace asio = boost::asio;
namespace sys  = boost::system;

Injector::Injector(asio::io_service& ios, string path_to_repo)
    : _backend(new Backend(ios, path_to_repo))
    , _db(new Db(*_backend, _backend->ipns_id()))
{
}

string Injector::ipns_id() const
{
    return _backend->ipns_id();
}

void Injector::insert_content(string url, const string& content, function<void(string)> cb)
{
    _backend->add( content
                 , [this, url = move(url), cb = move(cb)] (sys::error_code ec, string ipfs_id) {
                        auto ipfs_id_ = ipfs_id;

                        _db->update( move(url)
                                   , move(ipfs_id)
                                   , [cb = move(cb), ipfs_id = move(ipfs_id_)] {
                                         cb(ipfs_id);
                                     });
                   });
}

string Injector::insert_content(string url, const string& content, asio::yield_context yield)
{
    using handler_type = typename asio::handler_type
                           < asio::yield_context
                           , void(sys::error_code, string)>::type;

    handler_type handler(yield);
    asio::async_result<handler_type> result(handler);

    insert_content(move(url), content, [h = move(handler)](string v) mutable {
            h(sys::error_code(), move(v));
        });

    return result.get();
}

Injector::~Injector() {}
