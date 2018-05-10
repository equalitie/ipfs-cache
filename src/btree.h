#pragma once

#include <boost/optional.hpp>
#include <boost/asio/spawn.hpp>
#include <memory>
#include <map>
#include <iostream>
#include "namespaces.h"
#include "defer.h"

namespace ipfs_cache {

class BTree {
public:
    struct Node;

    using Key   = std::string;
    using Value = std::string;
    using Hash  = std::string;

    using CatOp = std::function<Value(const Hash&,  asio::yield_context)>;
    using AddOp = std::function<Hash (const Value&, asio::yield_context)>;

public:
    struct Node; // public, but opaque

public:
    BTree(CatOp = nullptr, AddOp = nullptr, size_t max_node_size = 512);

    boost::optional<Value> find(const Key&) const;
    void insert(const Key&, Value);

    bool check_invariants() const;

    ~BTree();

private:
     size_t _max_node_size;
     std::unique_ptr<Node> _root;

     CatOp _cat_op;
     AddOp _add_op;

     std::shared_ptr<bool> _was_destroyed;
};


} // namespace
