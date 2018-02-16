#include <ipfs_cache/client.h>
#include <ipfs_cache/error.h>

#include "backend.h"
#include "db.h"
#include "get_content.h"

using namespace std;
using namespace ipfs_cache;

namespace asio = boost::asio;
namespace sys  = boost::system;

Client::Client(boost::asio::io_service& ios, string ipns, string path_to_repo)
    : _backend(new Backend(ios, path_to_repo))
    , _db(new ClientDb(*_backend, path_to_repo, ipns))
{
}

CachedContent Client::get_content(string url, asio::yield_context yield)
{
    return ipfs_cache::get_content(*_db, url, yield);
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
