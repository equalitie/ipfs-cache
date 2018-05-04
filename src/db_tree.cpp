#include <boost/optional.hpp>
#include "db_tree.h"
#include "or_throw.h"

using namespace std;
using namespace ipfs_cache;

using Key   = DbTree::Key;
using Value = DbTree::Value;
using Hash  = DbTree::Hash;
using boost::optional;

static const size_t MAX_NODE_SIZE = 1000;

struct NodeId {
    explicit NodeId(Key&& k) : key(move(k)), is_inf(false) {}
    NodeId()                 :               is_inf(true)  {}

    Key key;
    bool is_inf = false;

    bool operator==(const Key& other_key) const {
        if (is_inf) return false;
        return key == other_key;
    }

    bool operator<(const NodeId& other) const {
        if (is_inf) {
            if (other.is_inf) return key < other.key;
            return false;
        }
        else if (other.is_inf) {
            return true;
        }
        return key < other.key;
    }

    bool operator<(const string& other) const {
        if (is_inf) return false;
        return key < other;
    }
};

struct NodeIdCompare {
    // https://www.fluentcpp.com/2017/06/09/search-set-another-type-key/
    using is_transparent = void;

    bool operator()(const NodeId& n1, const NodeId& n2) const {
        return n1 < n2;
    }

    bool operator()(const NodeId& n, const string& s) const {
        return n < s;
    }
};

struct DbTree::NodeEntry {
    Value value;
    unique_ptr<DbTree::Node> child;

    DbTree::Node* child_node() {
        if (!child) child = make_unique<Node>();
        return child.get();
    }
};

struct DbTree::Node {
    using Entries = map<NodeId, NodeEntry, NodeIdCompare>;

    Entries entries;

    optional<Node> insert(const Key&, Value);
    Value find(const Key&, asio::yield_context);
    optional<Node> split();

    bool is_leaf() const;

    Entries::iterator inf_entry();
    Entries::iterator lower_bound(const Key&);
};

bool DbTree::Node::is_leaf() const
{
    return entries.empty();
}

DbTree::Node::Entries::iterator DbTree::Node::inf_entry()
{
    if (entries.empty()) {
        return entries.insert(make_pair(NodeId(), NodeEntry{{}, nullptr})).first;
    }
    auto i = --entries.end();
    if (i->first.is_inf) return i;
    return entries.insert(make_pair(NodeId(), NodeEntry{{}, nullptr})).first;
}

// Return iterator with key greater or equal to the `key`
DbTree::Node::Entries::iterator DbTree::Node::lower_bound(const Key& key)
{
    auto i = entries.lower_bound(key);
    if (i != entries.end()) return i;
    return entries.insert(make_pair(NodeId(), NodeEntry{{}, nullptr})).first;
}

optional<DbTree::Node>
DbTree::Node::insert(const Key& key, Value value)
{
    if (!is_leaf()) {
        auto i = lower_bound(key); 

        if (i->first == key) {
            i->second.value = move(value);
            return boost::none;
        }

        auto& entry = i->second;
        auto new_node = entry.child_node()->insert(key, move(value));

        if (new_node) {
            assert(new_node->entries.size() == 2);

            auto& k1 = new_node->entries.begin()->first;
            auto& e1 = new_node->entries.begin()->second;
            auto& e2 = (++new_node->entries.begin())->second;

            entries.insert(make_pair(k1, std::move(e1)));

            for (auto& e : e2.child->entries) {
                entry.child->entries.insert(move(e));
            }
        }
    }
    else {
        entries[NodeId(string(key))] = NodeEntry{move(value), nullptr};
    }

    return split();
}

optional<DbTree::Node> DbTree::Node::split()
{
    if (entries.size() <= MAX_NODE_SIZE) {
        return boost::none;
    }

    size_t median = entries.size() / 2;
    bool fill_left = true;

    auto left_child = make_unique<Node>();
    Node ret;

    while(!entries.empty()) {
        if (fill_left && median-- == 0) {
            ret.entries.insert(move(*entries.begin()));
            ret.entries.begin()->second.child = move(left_child);
            fill_left = false;
        }
        else if (fill_left) {
            left_child->entries.insert(move(*entries.begin()));
        }
        else {
            ret.inf_entry()->second.child->entries.insert(move(*entries.begin()));
        }

        entries.erase(entries.begin());
    }

    return ret;
}

Value DbTree::Node::find(const Key& key, asio::yield_context yield)
{
    auto i = lower_bound(key);

    assert(i != entries.end());

    if (i->first == key) {
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
