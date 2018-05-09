#include "btree.h"
#include <iostream>
#include <map>

using namespace std;
using namespace ipfs_cache;

using Key   = BTree::Key;
using Value = BTree::Value;
using boost::optional;

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

ostream& operator<<(ostream& os, const NodeId& n) {
    if (n.is_inf) return os << "INF";
    return os << n.key;
}

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

struct BTree::NodeEntry {
    Value value;
    unique_ptr<BTree::Node> child;

    BTree::Node* child_node(size_t max_node_size) {
        if (!child) child = make_unique<Node>(max_node_size);
        return child.get();
    }
};

using Entries = map<NodeId, BTree::NodeEntry, NodeIdCompare>;

struct BTree::Node {
    Entries entries;
    size_t max_node_size;
    string ipfs_hash;

    Node(size_t max_node_size)
        : max_node_size(max_node_size) {}

    bool check_invariants() const;
    void print(ostream&, size_t depth) const;

    optional<Node> insert(const Key&, Value);
    optional<Value> find(const Key&);
    optional<Node> split();

    size_t size() const;
    pair<size_t,size_t> min_max_depth() const;
    bool is_leaf() const;

    void refresh_ipfs();

    Entries::iterator inf_entry();
    Entries::iterator lower_bound(const Key&);
};

//void BTree::Node::refresh_ipfs(asio::yield_context yield) {
//    if (!ipfs_hash.empty()) {
//        return;
//    }
//
//    for (auto& e : entries) {
//        if (e.second.child) {
//            e.second.child->refresh_ipfs();
//        }
//    }
//
//    Json json;
//
//    for (auto& e : entries) {
//        json[e.first.key]["data"] = e.second.value;
//
//        if (e.second.child) {
//            json[e.first.key]["child"] = e.second.child->ipfs_hash;
//        }
//    }
//
//    auto dump = json.dump();
//
//    sys::error_code ec;
//
//    Hash hash = add_op(dump.data(), dump.size(), yield[ec]);
//    if (ec) return or_throw(yield, ec);
//
//    ipfs_hash = move(hash);
//}

static ostream& operator<<(ostream& os, const BTree::Node&);
static ostream& operator<<(ostream& os, const Entries& es)
{
    os << "{";
    for (auto i = es.begin(); i != es.end(); ++i) {
        os << i->first;
        if (i->second.child) {
            os << ":" << *i->second.child;
        }

        if (next(i) != es.end()) os << " ";
    }
    return os << "}";
}

static ostream& operator<<(ostream& os, const BTree::Node& n)
{
    return os << n.entries;
}

size_t BTree::Node::size() const
{
    if (entries.empty()) return 0;
    if ((--entries.end())->first.is_inf) return entries.size() - 1;
    return entries.size();
}

bool BTree::Node::is_leaf() const
{
    for (auto& e: entries) {
        if (e.second.child) return false;
    }

    return true;
}

Entries::iterator BTree::Node::inf_entry()
{
    if (entries.empty()) {
        return entries.insert(make_pair(NodeId(), NodeEntry{{}, nullptr})).first;
    }
    auto i = --entries.end();
    if (i->first.is_inf) return i;
    return entries.insert(make_pair(NodeId(), NodeEntry{{}, nullptr})).first;
}

pair<size_t,size_t> BTree::Node::min_max_depth() const
{
    size_t min(1);
    size_t max(1);

    bool first = true;

    for (auto& e : entries) {
        if (!e.second.child) continue;
        auto mm = e.second.child->min_max_depth();
        if (first) {
            first = false;
            min = mm.first  + 1;
            max = mm.second + 1;
        }
        else {
            min = std::min(min, mm.first  + 1);
            max = std::max(max, mm.second + 1);
        }
    }

    return make_pair(min, max);
}

// Return iterator with key greater or equal to the `key`
Entries::iterator BTree::Node::lower_bound(const Key& key)
{
    auto i = entries.lower_bound(key);
    if (i != entries.end()) return i;
    return entries.insert(make_pair(NodeId(), NodeEntry{{}, nullptr})).first;
}

optional<BTree::Node>
BTree::Node::insert(const Key& key, Value value)
{
    if (!is_leaf()) {

        auto i = lower_bound(key); 

        if (i->first == key) {
            i->second.value = move(value);
            return boost::none;
        }

        auto& entry = i->second;
        auto new_node = entry.child_node(max_node_size)->insert(key, move(value));


        if (new_node) {
            assert(new_node->entries.size() == 2);

            auto& k1 = new_node->entries.begin()->first;
            auto& e1 = new_node->entries.begin()->second;
            auto& e2 = (++new_node->entries.begin())->second;

            auto j = entries.insert(make_pair(k1, std::move(e1))).first;

            entry.child->entries.clear();

            for (auto& e : e2.child->entries) {
                std::next(j)->second.child->entries.insert(move(e));
            }
        }
    }
    else {
        entries[NodeId(string(key))] = NodeEntry{move(value), nullptr};
    }

    return split();
}

optional<BTree::Node> BTree::Node::split()
{
    if (size() <= max_node_size) {
        return boost::none;
    }

    size_t median = size() / 2;
    bool fill_left = true;

    auto left_child = make_unique<Node>(max_node_size);
    Node ret(max_node_size);

    while(!entries.empty()) {
        if (fill_left && median-- == 0) {
            auto& e = *entries.begin();
            left_child->inf_entry()->second.child = move(e.second.child);
            e.second.child = move(left_child);
            ret.entries.insert(move(e));
            fill_left = false;
        }
        else if (fill_left) {
            left_child->entries.insert(move(*entries.begin()));
        }
        else {
            auto& ch = ret.inf_entry()->second.child;
            if (!ch) { ch = make_unique<Node>(max_node_size); }
            ch->entries.insert(move(*entries.begin()));
        }

        entries.erase(entries.begin());
    }

    return ret;
}

optional<Value> BTree::Node::find(const Key& key)
{
    auto i = lower_bound(key);

    assert(i != entries.end());

    if (i->first == key) {
        return i->second.value;
    }
    else {
        if (!i->second.child) return boost::none;
        return i->second.child->find(key);
    }
}

bool BTree::Node::check_invariants() const {
    if (size() > max_node_size) {
        return false;
    }

    auto mm = min_max_depth();

    if (mm.first != mm.second) {
        return false;
    }

    for (auto& e : entries) {
        if (!e.second.child) {
            continue;
        }

        for (auto& ee : e.second.child->entries) {
            if (!ee.first.is_inf && !(ee.first < e.first)) {
                return false;
            }
        }

        if (!e.second.child->check_invariants()) {
            return false;
        }
    }

    return true;
}

void BTree::Node::print(ostream& os, size_t depth) const
{
    string indent(2*depth, ' ');

    for (auto& e : entries) {
        os << indent << e.first << endl;

        if (e.second.child) {
            e.second.child->print(os, depth + 1);
        }
    }
}

void BTree::print(std::ostream& os) const
{
    if (!_root) { os << "{}"; return; }
    _root->print(os, 0);
}

BTree::BTree(size_t max_node_size)
    : _max_node_size(max_node_size)
{}

optional<BTree::Value> BTree::find(const Key& key)
{
    if (!_root) {
        return boost::none;
    }

    return _root->find(key);
}

void BTree::insert(const Key& key, Value value)
{
    if (!_root) _root = make_unique<Node>(_max_node_size);

    auto n = _root->insert(key, move(value));

    if (n) {
        *_root = move(*n);
    }

    assert(_root->check_invariants());
}

bool BTree::check_invariants() const
{
    if (!_root) return true;
    return _root->check_invariants();
}

BTree::~BTree() {}
