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
    , _was_destroyed(make_shared<bool>(false))
{
}

string Injector::ipns_id() const
{
    return _backend->ipns_id();
}

void Injector::insert_content_from_queue()
{
    if (_insert_queue.empty()) return;

    ++_job_count;

    auto e = move(_insert_queue.front());
    _insert_queue.pop();

    auto wd = _was_destroyed;

    auto value = move(e.value);

    _backend->add( value
                 , [this, e = move(e), wd]
                   (sys::error_code eca, string ipfs_id) {
                        if (*wd) return;

                        --_job_count;
                        insert_content_from_queue();

                        if (eca) {
                            return e.on_insert(eca, move(ipfs_id));
                        }

                        asio::spawn( _backend->get_io_service()
                                   , [ key     = move(e.key)
                                     , ipfs_id = move(ipfs_id)
                                     , ts      = e.ts
                                     , cb      = move(e.on_insert)
                                     , wd      = move(wd)
                                     , this
                                     ]
                                     (asio::yield_context yield) {
                                         if (*wd) return;

                                         sys::error_code ec;
                                         Json json;

                                         json["value"] = ipfs_id;
                                         json["ts"]    = boost::posix_time::to_iso_extended_string(ts) + 'Z';

                                         _db->update(move(key), json.dump(), yield[ec]);
                                         cb(ec, ipfs_id);
                                     });
                   });
}

void Injector::insert_content( string key
                             , const string& value
                             , function<void(sys::error_code, string)> cb)
{
    _insert_queue.push(
            InsertEntry{ move(key)
                       , move(value)
                       , boost::posix_time::microsec_clock::universal_time()
                       , move(cb)});

    if (_job_count >= _concurrency) {
        return;
    }

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

Injector::~Injector()
{
    *_was_destroyed = true;
}
