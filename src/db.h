#pragma once

#include <event2/event.h>
#include <string>
#include <queue>
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

private:
    void on_db_update(Json&& json);
    void replay_queued_tasks();

private:
    bool _had_update = false;
    Json _json;
    std::string _ipns;
    Backend& _backend;
    std::queue<std::function<void()>> _queued_tasks;
};

} // ipfs_cache namespace

