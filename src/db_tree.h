#pragma once

#include <boost/asio/spawn.hpp>
#include <namespaces.h>

namespace ipfs_cache {

class DbTree {
public:
    using Key   = std::string;
    using Value = std::string;
    using Hash  = std::string;

    using CatOp = std::function<Value(const Hash&,  asio::yield_context)>;
    using AddOp = std::function<Hash (const Value&, asio::yield_context)>;

public:
    DbTree(CatOp);

    Value find(const Key&, asio::yield_context);
    void insert(const Key&, const Value&, asio::yield_context);

private:
     CatOp _cat_op;
     AddOp _add_op;

     std::map<Key, Value> _sites;
};

} // namespace
