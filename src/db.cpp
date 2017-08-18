#include "db.h"

namespace ipfs_cache {

Db::Db(const std::string& serialized_db)
    : json(Json::parse(serialized_db))
{}

} // ipfs_cache namespace

