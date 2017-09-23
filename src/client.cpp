#include <ipfs_cache/client.h>

#include "backend.h"
#include "db.h"

using namespace std;
using namespace ipfs_cache;

namespace asio = boost::asio;
namespace sys  = boost::system;

Client::Client(boost::asio::io_service& ios, string ipns, string path_to_repo)
    : _backend(new Backend(ios, path_to_repo))
    , _db(new Db(*_backend, ipns))
{
}

void Client::get_content(string url, std::function<void(string)> cb)
{
    _db->query(url, [this, cb = move(cb)](string ipfs_id) {
         if (ipfs_id.empty()) {
            return cb(move(ipfs_id));
         }

        _backend->cat(ipfs_id, move(cb));
    }); 
}

string Client::get_content(string url, asio::yield_context yield)
{
    using handler_type = typename asio::handler_type
                           < asio::yield_context
                           , void(sys::error_code, string)>::type;

    handler_type handler(yield);
    asio::async_result<handler_type> result(handler);

    get_content(move(url), [h = move(handler)](string v) mutable {
            h(sys::error_code(), move(v));
        });

    return result.get();
}

Client::~Client() {}
