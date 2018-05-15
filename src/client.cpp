#include <ipfs_cache/client.h>
#include <ipfs_cache/error.h>

#include "backend.h"
#include "db.h"
#include "get_content.h"
#include "or_throw.h"

using namespace std;
using namespace ipfs_cache;

namespace asio = boost::asio;
namespace sys  = boost::system;

unique_ptr<Client> Client::build( asio::io_service& ios
                                , string ipns
                                , string path_to_repo
                                , function<void()>& cancel
                                , asio::yield_context yield)
{
    using ClientP = unique_ptr<Client>;

    bool canceled = false;

    cancel = [&canceled] {
        cout << "TODO: Canceling Client::build doesn't immediately stop "
             << "IO tasks" << endl;;

        canceled = true;
    };

    sys::error_code ec;
    auto backend = Backend::build(ios, path_to_repo, yield[ec]);

    cancel = nullptr;

    if (canceled) {
        ec = asio::error::operation_aborted;
    }

    if (ec) return or_throw<ClientP>(yield, ec);

    return ClientP(new Client(move(*backend), move(ipns), move(path_to_repo)));
}

Client::Client(Backend backend, string ipns, string path_to_repo)
    : _path_to_repo(move(path_to_repo))
    , _backend(new Backend(move(backend)))
    , _db(new ClientDb(*_backend, _path_to_repo, ipns))
{
}

Client::Client(boost::asio::io_service& ios, string ipns, string path_to_repo)
    : _path_to_repo(move(path_to_repo))
    , _backend(new Backend(ios, _path_to_repo))
    , _db(new ClientDb(*_backend, _path_to_repo, ipns))
{
}

string Client::ipfs_add(const string& data, asio::yield_context yield)
{
    return _backend->add(data, yield);
}

CachedContent Client::get_content(string url, asio::yield_context yield)
{
    return ipfs_cache::get_content(*_db, url, yield);
}

void Client::wait_for_db_update(boost::asio::yield_context yield)
{
    _db->wait_for_db_update(yield);
}

void Client::set_ipns(std::string ipns)
{
    _db.reset(new ClientDb(*_backend, _path_to_repo, move(ipns)));
}

std::string Client::id() const
{
    return _backend->ipns_id();
}

const string& Client::ipns() const
{
    return _db->ipns();
}

const string& Client::ipfs() const
{
    return _db->ipfs();
}

//const Json& Client::json_db() const
//{
//    return _db->json_db();
//}

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
