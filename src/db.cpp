#include "db.h"
#include "backend.h"
#include "republisher.h"
#include "timer.h"

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
    , _download_timer(make_unique<Timer>(_backend.get_io_service()))
{
    start_db_download();
}

void Db::update(string key, string value, function<void(sys::error_code)> cb)
{
    // An empty key will not add anything into the json structure but 
    // will still force the updating loop to start. This is useful when
    // the injector wants to upload an empty database.
    if (!key.empty()) {
        try {
            _json[key] = value;
        }
        catch(...) {
            assert(0);
        }
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
            cb(ecr, Json());
            return;
        }

        _backend.cat(ipfs_id, [this, cb = move(cb), d, ipfs_id](sys::error_code ecc, string content) {
            if (*d) return;

            _ipfs = ipfs_id;

            try {
                cb(ecc, Json::parse(content));
            }
            catch(...) {
                cb(error::make_error_code(error::invalid_db_format), Json());
            }
        });
    });
}

template<class F>
void Db::upload_database(const Json& json , F&& cb)
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
    auto& ios = get_io_service();

    auto i = _json.find(key);

    // We only ever store string values.
    if (i != _json.end() && i->is_string()) {
        ios.post(bind(move(cb), sys::error_code(), *i));
    }
    else {
        ios.post(bind(move(cb), make_error_code(error::key_not_found), ""));
    }
}

void Db::merge(const Json& remote_db)
{
    for (auto it = remote_db.begin(); it != remote_db.end(); ++it) {
        _json[it.key()] = it.value();
    }
}

void Db::start_db_download()
{
    cout << "Db::start_db_download" << endl;
    download_database(_ipns, [this](sys::error_code ec, Json json) {
        cout << "DB download: " << ec.message() << endl;
        if (ec) {
            _download_timer->start( chrono::seconds(5)
                                  , [this] { start_db_download(); });
            return;
        }
        on_db_download(move(json));
    });
}

void Db::on_db_download(Json&& json)
{
    merge(json);

    _download_timer->start( chrono::seconds(5)
                          , [this] { start_db_download(); });
}

asio::io_service& Db::get_io_service() {
    return _backend.get_io_service();
}

const Json& Db::json_db() const
{
    return _json;
}

Db::~Db() {
    *_was_destroyed = true;
}
