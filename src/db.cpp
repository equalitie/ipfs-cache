#include "db.h"
#include "backend.h"
#include "republisher.h"

#include <boost/asio/io_service.hpp>

#include <ipfs_cache/error.h>

#include <fstream>

using namespace std;
using namespace ipfs_cache;

static string path_to_db(const string& path_to_repo, const string& ipns)
{
    return path_to_repo + "/ipfs_cache_db." + ipns + ".json";
}

static Json load_db(const string& path_to_repo, const string& ipns)
{
    Json db;
    string path = path_to_db(path_to_repo, ipns);

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

static void save_db(const Json& db, const string& path_to_repo, const string& ipns)
{
    string path = path_to_db(path_to_repo, ipns);

    ofstream file(path, std::ofstream::trunc);

    if (!file.is_open()) {
        cerr << "ERROR: Saving " << path << endl;
        return;
    }

    file << setw(4) << db;
    file.close();
}

ClientDb::ClientDb(Backend& backend, string path_to_repo, string ipns)
    : _path_to_repo(move(path_to_repo))
    , _ipns(move(ipns))
    , _backend(backend)
    , _was_destroyed(make_shared<bool>(false))
    , _download_timer(_backend.get_io_service())
{
    _local_db = load_db(_path_to_repo, _ipns);
    auto d = _was_destroyed;

    asio::spawn(get_io_service(), [=](asio::yield_context yield) {
            if (*d) return;
            continuously_download_db(yield);
        });
}

InjectorDb::InjectorDb(Backend& backend, string path_to_repo)
    : _path_to_repo(move(path_to_repo))
    , _ipns(backend.ipns_id())
    , _backend(backend)
    , _republisher(new Republisher(_backend))
    , _has_callbacks(_backend.get_io_service())
    , _was_destroyed(make_shared<bool>(false))
{
    _local_db = load_db(_path_to_repo, _ipns);
    auto d = _was_destroyed;

    asio::spawn(get_io_service(), [=](asio::yield_context yield) {
            if (*d) return;
            continuously_upload_db(yield);
        });
}

static
void initialize_db(Json& json, const string& ipns)
{
    json["sites"] = Json::object();
}

static
string now_as_string() {
    auto entry_date = boost::posix_time::microsec_clock::universal_time();
    return boost::posix_time::to_iso_extended_string(entry_date) + 'Z';
}

static
boost::posix_time::ptime ptime_from_string(const string& s) {
    try {
        return boost::posix_time::from_iso_extended_string(s);
    } catch(...) {
        return boost::posix_time::ptime(boost::posix_time::not_a_date_time);
    }
}

void InjectorDb::update(string key, string content_hash, function<void(sys::error_code)> cb)
{
    if (_local_db == Json()) {
        initialize_db(_local_db, _ipns);
    }

    // An empty key will not add anything into the json structure but
    // will still force the updating loop to start. This is useful when
    // the injector wants to upload a database with no sites.
    if (!key.empty()) {
        try {
            _local_db["sites"][key] = {
                { "date", now_as_string() },
                { "link", content_hash }
            };
        }
        catch(...) {
            assert(0);
        }
    }

    // TODO: When the database get's big, this will become costly to do
    // on each update, thus we need to think of a smarter solution.
    save_db(_local_db, _path_to_repo, _ipns);

    _upload_callbacks.push_back(move(cb));
    _has_callbacks.notify_one();
}

void InjectorDb::continuously_upload_db(asio::yield_context yield)
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
        upload_database(_local_db, ec, yield);

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

string InjectorDb::upload_database( const Json& json
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

static CacheEntry query_(string key, const Json& db, sys::error_code& ec)
{
    CacheEntry entry;  // default: not a date/time, empty string

    auto sites_i = db.find("sites");

    if (sites_i == db.end() || !sites_i->is_object()) {
        ec = make_error_code(error::key_not_found);
        return entry;
    }

    auto item_i = sites_i->find(key);

    // We only ever store objects with "date" and "link" members.
    if (item_i == sites_i->end() || !item_i->is_object()) {
        ec = make_error_code(error::key_not_found);
        return entry;
    }

    auto date_i = item_i->find("date");

    boost::posix_time::ptime date;
    if (date_i == item_i->end() || !date_i->is_string() || (date = ptime_from_string(*date_i)).is_not_a_date_time()) {
        ec = make_error_code(error::malformed_db_entry);
        return entry;
    }

    auto link_i = item_i->find("link");

    // We only ever store string values.
    if (link_i == item_i->end() || !link_i->is_string()) {
        ec = make_error_code(error::malformed_db_entry);
        return entry;
    }

    entry.date = date;
    entry.content_hash = *link_i;
    return entry;
}

CacheEntry InjectorDb::query(string key, sys::error_code& ec)
{
    return query_(key, _local_db, ec);
}

CacheEntry ClientDb::query(string key, sys::error_code& ec)
{
    return query_(key, _local_db, ec);
}

void ClientDb::merge(const Json& remote_db)
{
    auto r_sites_i = remote_db.find("sites");

    if (r_sites_i == remote_db.end() || !r_sites_i->is_object()) {
        return;
    }

    _local_db["sites"] = *r_sites_i;
}

Json ClientDb::download_database( const string& ipns
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

void ClientDb::continuously_download_db(asio::yield_context yield)
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

        flush_db_update_callbacks(sys::error_code());

        // TODO: When the database get's big, this will become costly to do on
        // each download, thus we need to think of a smarter solution.
        save_db(_local_db, _path_to_repo, _ipns);

        _download_timer.expires_from_now(chrono::seconds(5));
        _download_timer.async_wait(yield[ec]);

        if (*d) return;
    }
}

void ClientDb::wait_for_db_update(asio::yield_context yield)
{
    using Handler = asio::handler_type<asio::yield_context,
          void(sys::error_code)>::type;

    Handler h(yield);
    asio::async_result<Handler> result(h);
    _on_db_update_callbacks.push([ h = move(h)
                                 , w = asio::io_service::work(get_io_service())
                                 ] (auto ec) mutable { h(ec); });
    result.get();
}

void ClientDb::flush_db_update_callbacks(const sys::error_code& ec)
{
    auto& q = _on_db_update_callbacks;

    while (!q.empty()) {
        auto c = move(q.front());
        q.pop();
        get_io_service().post([c = move(c), ec] () mutable { c(ec); });
    }
}

asio::io_service& ClientDb::get_io_service() {
    return _backend.get_io_service();
}

asio::io_service& InjectorDb::get_io_service() {
    return _backend.get_io_service();
}

const Json& ClientDb::json_db() const
{
    return _local_db;
}

ClientDb::~ClientDb() {
    *_was_destroyed = true;
    flush_db_update_callbacks(asio::error::operation_aborted);
}

InjectorDb::~InjectorDb() {
    *_was_destroyed = true;

    for (auto& cb : _upload_callbacks) {
        get_io_service().post([cb = move(cb)] {
                cb(asio::error::operation_aborted);
            });
    }
}
