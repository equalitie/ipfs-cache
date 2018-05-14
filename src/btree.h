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
    using Key   = std::string;
    using Value = std::string;
    using Hash  = std::string;

    using CatOp   = std::function<Value(const Hash&,  asio::yield_context)>;
    using AddOp   = std::function<Hash (const Value&, asio::yield_context)>;
    using UnpinOp = std::function<void (const Hash&,  asio::yield_context)>;

    struct Node; // public, but opaque

public:
    BTree( CatOp   = nullptr
         , AddOp   = nullptr
         , UnpinOp = nullptr
         , size_t max_node_size = 512);

    boost::optional<Value> find(const Key&, asio::yield_context);
    void insert(Key, Value, asio::yield_context);

    bool check_invariants() const;

    const std::string& root_hash() const { return _root_hash; }

    void load(Hash, asio::yield_context);

    ~BTree();

    void debug(bool v) { _debug = v; }

    size_t local_node_count() const;

private:
    void raw_insert(Key, Value, asio::yield_context);

    boost::optional<Value> find( const Hash&
                               , std::unique_ptr<Node>&
                               , const Key&
                               , const CatOp&
                               , asio::yield_context);

    void try_unpin(Hash&, asio::yield_context);

private:
    size_t _max_node_size;
    std::unique_ptr<Node> _root;
    std::string _root_hash;
    std::map<Key, Value> _insert_buffer;
    bool _is_inserting = false;

    CatOp _cat_op;
    AddOp _add_op;
    UnpinOp _unpin_op;

    std::shared_ptr<bool> _was_destroyed;

    bool _debug = false;
};


} // namespace
