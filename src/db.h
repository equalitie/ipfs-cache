#pragma once

#include <event2/event.h>
#include <string>
#include <queue>
#include <list>
#include <json.hpp>

namespace ipfs_cache {

class Backend;

class Db {
public:
    using Json = nlohmann::json;

    Db(Backend&, std::string ipns);

    void update(std::string key, std::string value, std::function<void()>);
    void query(std::string key, std::function<void(std::string)>);

    event_base* evbase() const;

    ~Db();

private:
    void on_db_update(Json&& json);
    void replay_queued_tasks();
    void merge(const Json&);
    void start_updating();

private:
    bool _is_uploading = false;
    bool _had_download = false;
    Json _json;
    std::string _ipns;
    Backend& _backend;
    std::list<std::function<void()>> _upload_callbacks;
    std::queue<std::function<void()>> _queued_tasks;
    std::shared_ptr<bool> _was_destroyed;
};

} // ipfs_cache namespace

