#include "db_tree.h"
#include "or_throw.h"

using namespace std;
using namespace ipfs_cache;

using Key   = DbTree::Key;
using Value = DbTree::Value;
using Hash  = DbTree::Hash;

static const size_t MAX_NODE_SIZE = 2;

struct DbTree::NodeEntry {
    Value value;
    unique_ptr<Node> child;
};

struct DbTree::Node {
    map<Key, NodeEntry> childs;
    unique_ptr<Node>    last_child;

    void insert(const Key&, Value);
    Value find(const Key&, asio::yield_context);
};

void DbTree::Node::insert(const Key& key, Value value)
{
    if (childs.size() < MAX_NODE_SIZE) {
        auto i = childs.find(key);

        if (i == childs.end()) {
            childs.insert(make_pair(key, NodeEntry{move(value), nullptr}));
        }
        else {
            i->second.value = move(value);
        }
    }
    else {
        auto i = childs.lower_bound(key); // Greater or equal to the key

        if (i == childs.end()) {
            if (!last_child) last_child = make_unique<Node>();
            last_child->insert(key, move(value));
        }
        else if (i->first == key) {
            i->second.value = move(value);
        }
        else {
            i->second.child->insert(key, move(value));
        }
    }
}

Value DbTree::Node::find(const Key& key, asio::yield_context yield)
{
    auto i = childs.lower_bound(key);

    if (i == childs.end()) {
        if (!last_child) return or_throw<Value>(yield, asio::error::not_found);
        return last_child->find(key, yield);
    }
    else if (i->first == key) {
        return i->second.value;
    }
    else {
        if (!i->second.child) return or_throw<Value>(yield, asio::error::not_found);
        return i->second.child->find(key, yield);
    }
}

DbTree::DbTree(CatOp cat_op, AddOp add_op)
    : _cat_op(move(cat_op))
    , _add_op(move(add_op))
{}

DbTree::Value DbTree::find(const Key& key, asio::yield_context yield)
{
    if (!_root) {
        return or_throw<Value>(yield, asio::error::not_found);
    }

    return _root->find(key, yield);
}

void DbTree::insert( const Key& key
                   , Value value
                   , asio::yield_context yield)
{
    if (!_add_op) {
        return or_throw(yield, asio::error::operation_not_supported);
    }

    sys::error_code ec;

    auto hash = _add_op(value, yield[ec]);
    if (ec) { return or_throw(yield, ec); }

    if (!_root) _root = make_unique<Node>();

    _root->insert(key, move(value));
}

DbTree::~DbTree() {}
