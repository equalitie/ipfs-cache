#pragma once

#include <boost/asio/spawn.hpp>
#include <set>
#include <namespaces.h>

namespace ipfs_cache {

class DbTree {
public:
    using Key   = std::string;
    using Value = std::string;
    using Hash  = std::string;

    using CatOp = std::function<Value(const Hash&,  asio::yield_context)>;
    using AddOp = std::function<Hash (const Value&, asio::yield_context)>;

private:
    struct Node;
    struct NodeEntry;

public:
    DbTree(CatOp, AddOp, size_t max_node_size = 512);

    Value find(const Key&, asio::yield_context);
    void insert(const Key&, Value, asio::yield_context);

    ~DbTree();

private:
    void rebalance();

private:
     CatOp _cat_op;
     AddOp _add_op;
     size_t _max_node_size;

     std::unique_ptr<Node> _root;
};

} // namespace
