#include <boost/asio/io_service.hpp>
#include <assert.h>
#include <iostream>
#include <chrono>

#include <ipfs_cache/injector.h>

#include "backend.h"
#include "db.h"
#include "get_content.h"

using namespace std;
using namespace ipfs_cache;

namespace asio = boost::asio;
namespace sys  = boost::system;

Injector::Injector(asio::io_service& ios, string path_to_repo)
    : _backend(new Backend(ios, path_to_repo))
    , _db(new InjectorDb(*_backend, path_to_repo))
{
    _db->update("", "", [] (sys::error_code) {});
}

string Injector::ipns_id() const
{
    return _backend->ipns_id();
}

void Injector::insert_content_from_queue()
{
    if (_insert_queue.empty()) return;

    auto e = move(_insert_queue.front());
    _insert_queue.pop();

    _backend->add( e.value
                 , [this, key = move(e.key), cb = move(e.on_insert)]
                   (sys::error_code eca, string ipfs_id) {
                        auto ipfs_id_ = ipfs_id;

                        --_job_count;
                        insert_content_from_queue();

                        if (eca) {
                            return cb(eca, move(ipfs_id_));
                        }

                        _db->update( move(key)
                                   , move(ipfs_id)
                                   , [cb = move(cb), ipfs_id = move(ipfs_id_)]
                                     (sys::error_code ecu) {
                                         cb(ecu, ipfs_id);
                                     });
                   });
}

void Injector::insert_content( string key
                             , const string& value
                             , function<void(sys::error_code, string)> cb)
{
    _insert_queue.push(InsertEntry{move(key), move(value), move(cb)});

    if (_job_count >= _concurrency) {
        return;
    }

    ++_job_count;
    insert_content_from_queue();
}

string Injector::insert_content(string key, const string& value, asio::yield_context yield)
{
    using handler_type = typename asio::handler_type
                           < asio::yield_context
                           , void(sys::error_code, string)>::type;

    handler_type handler(yield);
    asio::async_result<handler_type> result(handler);

    insert_content(move(key), value, [h = move(handler)] (sys::error_code ec, string v) mutable {
            h(ec, move(v));
        });

    return result.get();
}

CachedContent Injector::get_content(string url, asio::yield_context yield)
{
    return ipfs_cache::get_content(*_db, url, yield);
}

Injector::~Injector() {}
