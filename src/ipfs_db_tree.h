#pragma once

#include <json.hpp>
#include "btree.h"

namespace ipfs_cache {

class IpfsDbTree {
public:
    struct NodeData {
        std::string ipfs_hash;
    };

    using Key   = typename BTree<NodeData>::Key;
    using Value = typename BTree<NodeData>::Value;
    using Hash  = std::string;

    using CatOp = std::function<Value(const Hash&,  asio::yield_context)>;
    using AddOp = std::function<Hash (const Value&, asio::yield_context)>;

    //using Json = nlohman::json;

public:
    IpfsDbTree(CatOp cat, AddOp add);

    void insert(const Key&, Value, asio::yield_context);
    boost::optional<Value> find(const Key&, asio::yield_context);

    ~IpfsDbTree();

private:
    BTree<NodeData> _btree;

    CatOp _cat_op;
    AddOp _add_op;

    std::shared_ptr<bool> _was_destroyed;
};

inline
IpfsDbTree::IpfsDbTree(CatOp cat, AddOp add)
    : _btree(256)
    , _cat_op(std::move(cat))
    , _add_op(std::move(add))
    , _was_destroyed(std::make_shared<bool>(false))
{}

inline
void IpfsDbTree::insert(const Key& key, Value value, asio::yield_context)
{
    _btree.insert(key, std::move(value), [] (BTree<NodeData>::Node& n) {
            n.data.ipfs_hash.clear();
        });
}

inline
boost::optional<IpfsDbTree::Value>
IpfsDbTree::find(const Key& key, asio::yield_context)
{
    return _btree.find(key);
}

inline
IpfsDbTree::~IpfsDbTree()
{
    *_was_destroyed = true;
}

} // namespace
