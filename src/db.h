#pragma once

#include <string>
#include <json.hpp>

namespace ipfs_cache {

class Db {
public:
    using Json = nlohmann::json;

    Db() {}
    Db(const std::string&);
    Db(const Db&) = delete;

    Json json;
};

} // ipfs_cache namespace

