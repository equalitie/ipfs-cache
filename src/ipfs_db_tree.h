#pragma once

#include <json.hpp>
#include "btree.h"

namespace ipfs_cache {

class IpfsDbTree {
public:
    using Key   = BTree::Key;
    using Value = BTree::Value;
    using Hash  = std::string;

    using CatOp = std::function<Value(const Hash&,  asio::yield_context)>;
    using AddOp = std::function<Hash (const Value&, asio::yield_context)>;

    //using Json = nlohman::json;

public:
    IpfsDbTree(CatOp cat, AddOp add);

    void insert(const Key&, Value, asio::yield_context);
    boost::optional<Value> find(const Key&, asio::yield_context);

private:
    BTree _btree;

    CatOp _cat_op;
    AddOp _add_op;

    std::shared_ptr<bool> _was_destroyed;
};

inline
IpfsDbTree::IpfsDbTree(CatOp cat, AddOp add)
    : _btree(256)
    , _cat_op(std::move(cat))
    , _add_op(std::move(add))
{}

inline
void IpfsDbTree::insert(const Key& key, Value value, asio::yield_context)
{
    _btree.insert(key, std::move(value));
}

inline
boost::optional<IpfsDbTree::Value>
IpfsDbTree::find(const Key& key, asio::yield_context)
{
    return _btree.find(key);
}

} // namespace
