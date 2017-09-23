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
                 , [this, url = move(url), cb = move(cb)] (string ipfs_id) {
                        auto ipfs_id_ = ipfs_id;

                        _db->update( move(url)
                                   , move(ipfs_id)
                                   , [cb = move(cb), ipfs_id = move(ipfs_id_)] {
                                         cb(ipfs_id);
                                     });
                   });
}

Injector::~Injector() {}
