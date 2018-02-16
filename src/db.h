#pragma once

#include <boost/system/error_code.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>
#include <queue>
#include <list>
#include <json.hpp>

#include "namespaces.h"
#include "condition_variable.h"

namespace boost { namespace asio {
    class io_service;
}}

namespace ipfs_cache {

class Backend;
class Republisher;
using Json = nlohmann::json;

struct CacheEntry {
    // Entry time stamp, not a date/time for missing or invalid entries.
    boost::posix_time::ptime ts;
    // Entry value (IPFS content hash).
    std::string content_hash;
};

class ClientDb {
    using OnDbUpdate = std::function<void(const sys::error_code&)>;

public:
    ClientDb(Backend&, std::string path_to_repo, std::string ipns);

    CacheEntry query(std::string key, sys::error_code& ec);

    boost::asio::io_service& get_io_service();

    const Json& json_db() const;
    const std::string& ipns() const { return _ipns; }
    const std::string& ipfs() const { return _ipfs; }

    void wait_for_db_update(boost::asio::yield_context);

    Backend& backend() { return _backend; }

    ~ClientDb();

private:
    void merge(const Json&);

    Json download_database(const std::string& ipns, sys::error_code&, asio::yield_context);
    void continuously_download_db(asio::yield_context);

    void flush_db_update_callbacks(const sys::error_code&);

private:
    const std::string _path_to_repo;
    Json _local_db;
    std::string _ipns;
    std::string _ipfs; // Last known
    Backend& _backend;
    std::shared_ptr<bool> _was_destroyed;
    asio::steady_timer _download_timer;
    std::queue<OnDbUpdate> _on_db_update_callbacks;
};

class InjectorDb {
public:
    InjectorDb(Backend&, std::string path_to_repo);

    void update( std::string key, std::string content_hash
               , std::function<void(sys::error_code)>);

    CacheEntry query(std::string key, sys::error_code& ec);

    boost::asio::io_service& get_io_service();

    const std::string& ipns() const { return _ipns; }

    Backend& backend() { return _backend; }

    ~InjectorDb();

private:
    std::string upload_database(const Json&, sys::error_code&, asio::yield_context);
    void continuously_upload_db(asio::yield_context);

private:
    const std::string _path_to_repo;
    Json _local_db;
    std::string _ipns;
    Backend& _backend;
    std::unique_ptr<Republisher> _republisher;
    ConditionVariable _has_callbacks;
    std::list<std::function<void(sys::error_code)>> _upload_callbacks;
    std::shared_ptr<bool> _was_destroyed;
};

} // ipfs_cache namespace

