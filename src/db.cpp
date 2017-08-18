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
{
    fetch_database(_backend, _ipns, [this](Json json) {
        on_db_update(move(json));
    });
}

void Db::update(string key, string value, function<void()> cb)
{
    if (!_had_update) {
        _queued_tasks.push(
            [ this
            , key   = move(key)
            , value = move(value)
            , cb    = move(cb)] {
            update(move(key), move(value), move(cb));
        });
        return;
    }

    try {
        _json[key] = value;
        string str = _json.dump();

        _backend.add((uint8_t*) str.data(), str.size(),
            [this, cb = move(cb)](string db_ipfs_id) {
                _backend.publish(move(db_ipfs_id), cb);
            });
    }
    catch(...) {
        assert(0);
    }
}

void Db::query(string key, function<void(string)> cb)
{
    if (!_had_update) {
        _queued_tasks.push([this, key = move(key), cb = move(cb)] {
            query(move(key), move(cb));
        });
        return;
    }

    auto   v = _json[key];
    string s = v.is_string() ? v : "";

    dispatch(evbase(), [cb = move(cb), s = move(s)] {
        cb(s);
    });
}

void Db::on_db_update(Json&& json)
{
    _json = move(json);

    cerr << ">>> " << _json.dump() << endl;

    if (!_had_update) {
        _had_update = true;
        replay_queued_tasks();
    }
}

void Db::replay_queued_tasks()
{
    auto tasks = move(_queued_tasks);
    
    while (!tasks.empty()) {
        auto t = move(tasks.front());
        tasks.pop();
        t();
    }
}

event_base* Db::evbase() const {
    return _backend.evbase();
}

} // ipfs_cache namespace

