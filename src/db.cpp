#include "db.h"
#include "backend.h"
#include "republisher.h"

#include <boost/asio/io_service.hpp>

using namespace std;
using namespace ipfs_cache;

namespace asio = boost::asio;

Db::Db(Backend& backend, string ipns)
    : _ipns(move(ipns))
    , _backend(backend)
    , _republisher(new Republisher(_backend))
    , _was_destroyed(make_shared<bool>(false))
{
    download_database(_ipns, [this](Json json) {
        on_db_update(move(json));
    });
}

void Db::update(string key, string value, function<void()> cb)
{
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

    upload_database(_json, [this, last_i] {
        _is_uploading = false;

        auto& cbs = _upload_callbacks;
        auto  destroyed = _was_destroyed;

        while (!cbs.empty()) {
            bool is_last = cbs.begin() == last_i;
            auto cb = move(cbs.front());
            cbs.erase(cbs.begin());
            cb();
            if (*destroyed) return;
            if (is_last) break;
        }

        start_updating();
    });
}

template<class F>
void Db::download_database(const string& ipns, F&& cb) {
    auto d = _was_destroyed;

    _backend.resolve(ipns, [this, cb = forward<F>(cb), d](string ipfs_id) {
        if (*d) return;

        if (ipfs_id.size() == 0) {
            cb(Db::Json());
            return;
        }

        _backend.cat(ipfs_id, [this, cb = move(cb), d](vector<char> content) {
            if (*d) return;

            try {
                cb(Db::Json::parse(content));
            }
            catch(...) {
                cb(Db::Json());
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
        [ this, cb = forward<F>(cb), d] (string db_ipfs_id) {
            if (*d) return;
            _republisher->publish(move(db_ipfs_id) , move(cb));
        });
}

void Db::query(string key, function<void(string)> cb)
{
    if (!_had_download) {
        _queued_tasks.push(bind(&Db::query, this, move(key), move(cb)));
        return;
    }

    auto   v = _json[key];
    string s = v.is_string() ? v : "";

    get_io_service().post(bind(move(cb), move(s)));
}

void Db::merge(const Json& remote_db)
{
    for (auto it = remote_db.begin(); it != remote_db.end(); ++it) {
        _json[it.key()] = it.value();
    }
}

void Db::on_db_update(Json&& json)
{
    merge(json);

    if (!_had_download) {
        _had_download = true;
        replay_queued_tasks();
    }
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
