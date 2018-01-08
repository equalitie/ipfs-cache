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
    , _db(new Db(*_backend, true, path_to_repo, ipns))
{
}

void Client::get_content(string url, function<void(sys::error_code, Json)> cb)
{
    auto& ios = _backend->get_io_service();

    sys::error_code ec;

    string ipfs_id = _db->query(url, ec);

    if (!ec && ipfs_id.empty()) {
        ec = error::key_not_found;
    }

    if (ec) {
        return ios.post([cb = move(cb), ec] { cb(ec, Json()); });
    }

    _backend->cat(ipfs_id, [cb = move(cb)] (sys::error_code ecc, string s) {
            if (ecc) {
                return cb(ecc, move(s));
            }

            try {
                auto json = Json::parse(s);
                cb(ecc, move(json));
            }
            catch (...) {
                cb(make_error_code(error::error_parsing_json), Json());
            }
        });
}

Json Client::get_content(string url, asio::yield_context yield)
{
    using handler_type = typename asio::handler_type
                           < asio::yield_context
                           , void(sys::error_code, Json)>::type;

    handler_type handler(yield);
    asio::async_result<handler_type> result(handler);

    get_content(move(url), handler);

    return result.get();
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

Client::~Client() {}
