#pragma once

#include <boost/system/error_code.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
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
class Timer;
using Json = nlohmann::json;

class Db {
public:
    Db(Backend&, bool is_client, std::string path_to_repo, std::string ipns);

    void update( std::string key, std::string value
               , std::function<void(sys::error_code)>);

    std::string query(std::string key, sys::error_code& ec);

    boost::asio::io_service& get_io_service();

    const Json& json_db() const;
    const std::string& ipns() const { return _ipns; }
    const std::string& ipfs() const { return _ipfs; }

    ~Db();

private:
    void merge(const Json&);

    Json download_database(const std::string& ipns, sys::error_code&, asio::yield_context);

    std::string upload_database(const Json&, sys::error_code&, asio::yield_context);

    void continuously_upload_db(asio::yield_context);
    void continuously_download_db(asio::yield_context);

private:
    const bool _is_client;
    const std::string _path_to_repo;
    Json _json;
    std::string _ipns;
    std::string _ipfs; // Last known
    Backend& _backend;
    std::unique_ptr<Republisher> _republisher;
    ConditionVariable _has_callbacks;
    std::list<std::function<void(sys::error_code)>> _upload_callbacks;
    std::shared_ptr<bool> _was_destroyed;
    asio::steady_timer _download_timer;
};

} // ipfs_cache namespace

