#include "db.h"
#include "backend.h"
#include "republisher.h"

#include <boost/asio/io_service.hpp>

#include <ipfs_cache/timer.h>
#include <ipfs_cache/error.h>

#include <fstream>

using namespace std;
using namespace ipfs_cache;

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
    , _has_callbacks(_backend.get_io_service())
    , _was_destroyed(make_shared<bool>(false))
    , _download_timer(_backend.get_io_service())
{
    _json = load_db(_path_to_repo);

    auto& ios = get_io_service();

    auto d = _was_destroyed;

    if (_is_client) {
        asio::spawn(ios, [=](asio::yield_context yield) {
                if (*d) return;
                continuously_download_db(yield);
            });
    }

    asio::spawn(ios, [=](asio::yield_context yield) {
            if (*d) return;
            continuously_upload_db(yield);
        });
}

static
void initialize_db(Json& json, const string& ipns)
{
    json["ipns"]  = ipns;
    json["sites"] = Json::object();
}

void Db::update(string key, string value, function<void(sys::error_code)> cb)
{
    if (_json == Json()) {
        initialize_db(_json, _ipns);
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
    _has_callbacks.notify_one();
}

void Db::continuously_upload_db(asio::yield_context yield)
{
    auto wd = _was_destroyed;

    sys::error_code ec;

    while(true)
    {
        if (_upload_callbacks.empty()) {
            _has_callbacks.wait(yield[ec]);
            if (*wd || ec) return;
        }

        assert(!_upload_callbacks.empty());

        auto last_i = --_upload_callbacks.end();

        sys::error_code ec;
        upload_database(_json, ec, yield);

        if (*wd || ec) return;

        auto& cbs = _upload_callbacks;

        while (!cbs.empty()) {
            bool is_last = cbs.begin() == last_i;
            auto cb = move(cbs.front());
            cbs.erase(cbs.begin());
            cb(ec);
            if (*wd) return;
            if (is_last) break;
        }
    }
}

string Db::upload_database( const Json& json
                          , sys::error_code& ec
                          , asio::yield_context yield)
{
    auto d = _was_destroyed;
    auto dump = json.dump();

    string db_ipfs_id = _backend.add((uint8_t*) dump.data(), dump.size(), yield[ec]);
    if (*d || ec) return string();

    _republisher->publish(move(db_ipfs_id), yield[ec]);
    return db_ipfs_id;
}

string Db::query(string key, sys::error_code& ec)
{
    auto sites_i = _json.find("sites");

    if (sites_i == _json.end() || !sites_i->is_object()) {
        ec = make_error_code(error::key_not_found);
        return "";
    }

    auto i = sites_i->find(key);

    // We only ever store string values.
    if (i == sites_i->end() || !i->is_string()) {
        ec = make_error_code(error::key_not_found);
        return "";
    }

    return *i;
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

Json Db::download_database( const string& ipns
                          , sys::error_code& ec
                          , asio::yield_context yield) {
    auto d = _was_destroyed;

    auto ipfs_id = _backend.resolve(ipns, yield[ec]);

    if (*d) { ec = asio::error::operation_aborted; }
    if (ec) return Json();

    assert(ipfs_id.size());

    string content = _backend.cat(ipfs_id, yield[ec]);

    if (*d) { ec = asio::error::operation_aborted; }
    if (ec) return Json();

    _ipfs = ipfs_id;

    try {
        return Json::parse(content);
    } catch(...) {
        ec = error::make_error_code(error::invalid_db_format);
        return Json();
    }
}

void Db::continuously_download_db(asio::yield_context yield)
{
    auto d = _was_destroyed;

    while(true) {
        sys::error_code ec;

        Json db = download_database(_ipns, ec, yield);
        if (*d) return;

        if (ec) {
            _download_timer.expires_from_now(chrono::seconds(5));
            _download_timer.async_wait(yield[ec]);

            if (*d) return;
            continue;
        }

        merge(db);

        // TODO: When the database get's big, this will become costly to do on
        // each download, thus we need to think of a smarter solution.
        save_db(_json, _path_to_repo);

        _download_timer.expires_from_now(chrono::seconds(5));
        _download_timer.async_wait(yield[ec]);

        if (*d) return;
    }
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

    for (auto& cb : _upload_callbacks) {
        get_io_service().post([cb = move(cb)] {
                cb(asio::error::operation_aborted);
            });
    }
}
