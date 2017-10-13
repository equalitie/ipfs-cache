#include "db.h"
#include "backend.h"
#include "republisher.h"

#include <boost/asio/io_service.hpp>

#include <ipfs_cache/timer.h>
#include <ipfs_cache/error.h>

#include <fstream>

using namespace std;
using namespace ipfs_cache;

namespace asio = boost::asio;
namespace sys  = boost::system;

static Json load_db(const string& path_to_repo)
{
    Json db;
    string path = path_to_repo + "/ipfs_cache_db.json";

    ifstream file(path);

    if (!file.is_open()) {
        cerr << "Warning: Couldn't open " << path << endl;
        return db;
    }

    try {
        file >> db;
    }
    catch (const std::exception& e) {
        cerr << "ERROR: parsing " << path << ": " << e.what() << endl;
    }

    return db;
}

static void save_db(const Json& db, const string& path_to_repo)
{
    string path = path_to_repo + "/ipfs_cache_db.json";

    ofstream file(path, std::ofstream::trunc);

    if (!file.is_open()) {
        cerr << "ERROR: Saving " << path << endl;
        return;
    }

    file << setw(4) << db;
    file.close();
}

Db::Db(Backend& backend, bool is_client, string path_to_repo, string ipns)
    : _is_client(is_client)
    , _path_to_repo(move(path_to_repo))
    , _ipns(move(ipns))
    , _backend(backend)
    , _republisher(new Republisher(_backend))
    , _was_destroyed(make_shared<bool>(false))
    , _download_timer(make_unique<Timer>(_backend.get_io_service()))
{
    _json = load_db(_path_to_repo);

    if (_is_client)
        start_db_download();
}

void Db::initialize(Json& json)
{
    json["ipns"] = _ipns;
    json["sites"] = Json::object();
}

void Db::update(string key, string value, function<void(sys::error_code)> cb)
{
    if (_json == Json()) {
        initialize(_json);
    }

    // An empty key will not add anything into the json structure but 
    // will still force the updating loop to start. This is useful when
    // the injector wants to upload a database with no sites.
    if (!key.empty()) {
        try {
            _json["sites"][key] = value;
        }
        catch(...) {
            assert(0);
        }
    }

    // TODO: When the database get's big, this will become costly to do
    // on each update, thus we need to think of a smarter solution.
    save_db(_json, _path_to_repo);

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
            cb(ecr, ipfs_id, Json());
            return;
        }

        _backend.cat(ipfs_id, [this, cb = move(cb), d, ipfs_id](sys::error_code ecc, string content) {
            if (*d) return;

            _ipfs = ipfs_id;

            try {
                cb(ecc, ipfs_id, Json::parse(content));
            }
            catch(...) {
                cb(error::make_error_code(error::invalid_db_format), ipfs_id, Json());
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

    auto sites_i = _json.find("sites");

    if (sites_i == _json.end() || !sites_i->is_object()) {
        return ios.post(bind(move(cb), make_error_code(error::key_not_found), ""));
    }

    auto i = sites_i->find(key);

    // We only ever store string values.
    if (i == sites_i->end() || !i->is_string()) {
        return ios.post(bind(move(cb), make_error_code(error::key_not_found), ""));
    }

    ios.post(bind(move(cb), sys::error_code(), *i));
}

void Db::merge(const Json& remote_db)
{
    auto r_ipns_i = remote_db.find("ipns");
    auto l_ipns_i = _json.find("ipns");

    if (l_ipns_i == _json.end()) {
        _json["ipns"] = r_ipns_i.value();
    }

    auto r_sites_i = remote_db.find("sites");

    if (r_sites_i == remote_db.end() || !r_sites_i->is_object()) {
        return;
    }

    auto l_sites_i = _json.find("sites");

    if (l_sites_i == _json.end() || !l_sites_i->is_object()) {
        // XXX: Can these two lines be done in one command?
        _json["sites"] = Json::object();
        l_sites_i = _json.find("sites");
    }

    for (auto it = r_sites_i->begin(); it != r_sites_i->end(); ++it) {
        if (!_is_client && l_sites_i->find(it.key()) != l_sites_i->end()) {
            // If we're the injector and we already have the value
            // then we likely have a more recent version.
            continue;
        }
        (*l_sites_i)[it.key()] = it.value();
    }
}

void Db::start_db_download()
{
    download_database(_ipns
                     , [this](sys::error_code ec, string ipfs_id, Json json) {
        if (ec) {
            cout << "DB download failed: " << ec.message() << endl;

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

    // TODO: When the database get's big, this will become costly to do
    // on each download, thus we need to think of a smarter solution.
    save_db(_json, _path_to_repo);

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
