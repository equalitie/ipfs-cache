#include "db.h"
#include "backend.h"
#include "dispatch.h"

using namespace std;

namespace ipfs_cache {

template<class F>
static void fetch_database(Backend& backend, const string& ipns, F&& cb) {
    backend.resolve(ipns, [&backend, cb = forward<F>(cb)](string ipfs_id) {
        if (ipfs_id.size() == 0) {
            cb(Db::Json());
            return;
        }

        backend.cat(ipfs_id, [cb = move(cb)](string content) {
            try {
                cb(Db::Json::parse(content));
            }
            catch(...) {
                cb(Db::Json());
            }
        });
    });
}

Db::Db(Backend& backend, string ipns)
    : _ipns(move(ipns))
    , _backend(backend)
    , _was_destroyed(make_shared<bool>(false))
{
    fetch_database(_backend, _ipns, [this](Json json) {
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
    if (_upload_callbacks.empty()) return;

    if (_is_uploading) return;
    _is_uploading = true;

    auto last_i = --_upload_callbacks.end();

    _backend.add(_json.dump(),
        [this, last_i](string db_ipfs_id) {
            _backend.publish(move(db_ipfs_id), [this, last_i] {
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

    dispatch(evbase(), bind(move(cb), move(s)));
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

event_base* Db::evbase() const {
    return _backend.evbase();
}

Db::~Db() {
    *_was_destroyed = true;
}

} // ipfs_cache namespace

