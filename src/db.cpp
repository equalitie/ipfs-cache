#include "db.h"
#include "backend.h"
#include "republisher.h"

#include <boost/asio/io_service.hpp>

#include <ipfs_cache/error.h>

using namespace std;
using namespace ipfs_cache;

namespace asio = boost::asio;
namespace sys  = boost::system;

Db::Db(Backend& backend, string ipns)
    : _ipns(move(ipns))
    , _backend(backend)
    , _republisher(new Republisher(_backend))
    , _was_destroyed(make_shared<bool>(false))
{
    start_db_download();
}

void Db::update(string key, string value, function<void(sys::error_code)> cb)
{
    if (_failed_download) {
        // Database download already failed, invoke callback straight away.
        return get_io_service().post(bind(move(cb), error::make_error_code(error::db_download_failed)));
    }

    if (!_had_download) {
        return _queued_tasks.push(bind(&Db::update
                                      , this
                                      , move(key)
                                      , move(value)
                                      , move(cb)));
    }

    try {
        _json[key] = value;
    }
    catch(...) {
        assert(0);
    }

    _upload_callbacks.push_back(move(cb));

    start_updating();
}

void Db::start_updating()
{
    // Not having upload callbacks means we have nothing to update.
    if (_upload_callbacks.empty()) return;

    if (_is_uploading) return;
    _is_uploading = true;

    auto last_i = --_upload_callbacks.end();

    upload_database(_json, [this, last_i] (sys::error_code ec) {
        _is_uploading = false;

        auto& cbs = _upload_callbacks;
        auto  destroyed = _was_destroyed;

        while (!cbs.empty()) {
            bool is_last = cbs.begin() == last_i;
            auto cb = move(cbs.front());
            cbs.erase(cbs.begin());
            cb(ec);
            if (*destroyed) return;
            if (is_last) break;
        }

        start_updating();
    });
}

template<class F>
void Db::download_database(const string& ipns, F&& cb) {
    auto d = _was_destroyed;

    _backend.resolve(ipns, [this, cb = forward<F>(cb), d](sys::error_code ecr, string ipfs_id) {
        if (*d) return;

        if (ecr || ipfs_id.size() == 0) {
            cb(ecr, Db::Json());
            return;
        }

        _backend.cat(ipfs_id, [this, cb = move(cb), d](sys::error_code ecc, string content) {
            if (*d) return;

            try {
                cb(ecc, Db::Json::parse(content));
            }
            catch(...) {
                cb(error::make_error_code(error::invalid_db_format), Db::Json());
            }
        });
    });
}

template<class F>
void Db::upload_database(const Db::Json& json , F&& cb)
{
    auto d = _was_destroyed;
    auto dump = json.dump();

    _backend.add((uint8_t*) dump.data(), dump.size(),
        [ this, cb = forward<F>(cb), d] (sys::error_code ec, string db_ipfs_id) {
            if (*d) return;
            if (ec)
                return cb(ec);
            _republisher->publish(move(db_ipfs_id) , move(cb));
        });
}

void Db::query(string key, function<void(sys::error_code, string)> cb)
{
    if (_failed_download) {
        // Database download already failed, invoke callback straight away.
        return get_io_service().post(bind(move(cb), error::make_error_code(error::db_download_failed), ""));
    }

    if (!_had_download) {
        _queued_tasks.push(bind(&Db::query, this, move(key), move(cb)));
        return;
    }

    auto   v = _json[key];
    string s = v.is_string() ? v : "";

    get_io_service().post(bind(move(cb), sys::error_code(), move(s)));
}

void Db::merge(const Json& remote_db)
{
    for (auto it = remote_db.begin(); it != remote_db.end(); ++it) {
        _json[it.key()] = it.value();
    }
}

void Db::start_db_download()
{
    if (_failed_download)  // database download already failed
        return;

    download_database(_ipns, [this](sys::error_code ec, Json json) {
        if (ec) {  // database download failed, flag this
            _failed_download = true;
            return;
        }
        on_db_update(move(json));
    });
}

void Db::on_db_update(Json&& json)
{
    merge(json);

    if (!_had_download) {
        _had_download = true;
        replay_queued_tasks();
    }

    // TODO: The 't' should be a member so it can be destroyed (canceled)
    // in the destructor.
    auto d = _was_destroyed;
    auto t = make_shared<boost::asio::steady_timer>(_backend.get_io_service());
    t->expires_from_now(std::chrono::seconds(5));
    t->async_wait([this, t,d](boost::system::error_code ec) {
            if (*d) return;
            start_db_download();
        });
}

void Db::replay_queued_tasks()
{
    auto tasks = move(_queued_tasks);
    auto destroyed = _was_destroyed;
    
    while (!tasks.empty()) {
        auto t = move(tasks.front());
        tasks.pop();
        t();
        if (*destroyed) return;
    }
}

asio::io_service& Db::get_io_service() {
    return _backend.get_io_service();
}

Db::~Db() {
    *_was_destroyed = true;
}
