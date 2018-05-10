#pragma once

#include "namespaces.h"
#include "btree.h"
#include <boost/asio/spawn.hpp>

namespace ipfs_cache {

class IpfsDbTree {
public:
    struct NodeData {
        std::string ipfs_hash;
    };

    using Tree  = BTree<NodeData>;
    using Key   = typename Tree::Key;
    using Value = typename Tree::Value;
    using Hash  = std::string;

    using CatOp = std::function<Value(const Hash&,  asio::yield_context)>;
    using AddOp = std::function<Hash (const Value&, asio::yield_context)>;

public:
    IpfsDbTree(CatOp cat, AddOp add);

    void insert(const Key&, Value, asio::yield_context);
    boost::optional<Value> find(const Key&, asio::yield_context);

    ~IpfsDbTree();

private:
    void update_ipfs(Tree::Node*, asio::yield_context);

private:
    Tree _btree;

    CatOp _cat_op;
    AddOp _add_op;

    std::shared_ptr<bool> _was_destroyed;
};

} // namespace
