#include <ipfs_cache/client.h>
#include <ipfs_cache/error.h>

#include "backend.h"
#include "db.h"

using namespace std;
using namespace ipfs_cache;

namespace asio = boost::asio;
namespace sys  = boost::system;

Client::Client(boost::asio::io_service& ios, string ipns, string path_to_repo)
    : _backend(new Backend(ios, path_to_repo))
    , _db(new ClientDb(*_backend, path_to_repo, ipns))
{
}

void Client::get_content(string url, function<void(sys::error_code, CachedContent)> cb)
{
    auto& ios = _backend->get_io_service();

    sys::error_code ec;

    CacheEntry entry = _db->query(url, ec);

    if (!ec && entry.ts.is_not_a_date_time()) {
        ec = asio::error::not_found;
    }

    if (ec) {
        return ios.post([cb = move(cb), ec] { cb(ec, CachedContent()); });
    }

    _backend->cat(entry.content_hash, [cb = move(cb), ts = entry.ts] (sys::error_code ecc, string s) {
            if (ecc) {
                return cb(ecc, CachedContent());
            }

            CachedContent cont({ts, s});
            cb(ecc, move(cont));
        });
}

CachedContent Client::get_content(string url, asio::yield_context yield)
{
    using handler_type = typename asio::handler_type
                           < asio::yield_context
                           , void(sys::error_code, CachedContent)>::type;

    handler_type handler(yield);
    asio::async_result<handler_type> result(handler);

    get_content(move(url), handler);

    return result.get();
}

void Client::wait_for_db_update(boost::asio::yield_context yield)
{
    _db->wait_for_db_update(yield);
}

const string& Client::ipns() const
{
    return _db->ipns();
}

const string& Client::ipfs() const
{
    return _db->ipfs();
}

const Json& Client::json_db() const
{
    return _db->json_db();
}

Client::Client(Client&& other)
    : _backend(move(other._backend))
    , _db(move(other._db))
{}

Client& Client::operator=(Client&& other)
{
    _backend = move(other._backend);
    _db = move(other._db);
    return *this;
}

Client::~Client() {}
