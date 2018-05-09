#include "btree.h"
#include <iostream>
#include <map>

using namespace std;
using namespace ipfs_cache;

using Key   = BTree::Key;
using Value = BTree::Value;
using boost::optional;

// boost::none represents the last entry in a node
// (i.e. the entry which is "bigger" than any Key)
using NodeId = boost::optional<Key>;

ostream& operator<<(ostream& os, const NodeId& n) {
    if (n == boost::none) return os << "INF";
    return os << *n;
}

struct NodeIdCompare {
    // https://www.fluentcpp.com/2017/06/09/search-set-another-type-key/
    using is_transparent = void;

    bool operator()(const NodeId& n1, const NodeId& n2) const {
        using boost::none;

        if (n1 == none) {
            return false;
        }
        else if (n2 == none) {
            return true;
        }
        return *n1 < *n2;
    }

    bool operator()(const NodeId& n, const string& s) const {
        using boost::none;

        if (n == none) {
            return false;
        }

        return *n < s;
    }
};

struct BTree::NodeEntry {
    Value value;
    unique_ptr<BTree::Node> child;
};

using Entries = map<NodeId, BTree::NodeEntry, NodeIdCompare>;

struct BTree::Node : public Entries {
    size_t max_node_size;

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

    iterator inf_entry();
    iterator lower_bound(const Key&);
};

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
    return os << static_cast<const Entries&>(n);
}

size_t BTree::Node::size() const
{
    if (Entries::empty()) return 0;
    if ((--Entries::end())->first == boost::none) return Entries::size() - 1;
    return Entries::size();
}

bool BTree::Node::is_leaf() const
{
    for (auto& e: static_cast<const Entries&>(*this)) {
        if (e.second.child) return false;
    }

    return true;
}

Entries::iterator BTree::Node::inf_entry()
{
    if (Entries::empty()) {
        return Entries::insert(make_pair(NodeId(), NodeEntry{{}, nullptr})).first;
    }
    auto i = --Entries::end();
    if (i->first == boost::none) return i;
    return Entries::insert(make_pair(NodeId(), NodeEntry{{}, nullptr})).first;
}

pair<size_t,size_t> BTree::Node::min_max_depth() const
{
    size_t min(1);
    size_t max(1);

    bool first = true;

    for (auto& e : *this) {
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
    auto i = Entries::lower_bound(key);
    if (i != Entries::end()) return i;
    return Entries::insert(make_pair(NodeId(), NodeEntry{{}, nullptr})).first;
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
        if (!entry.child) entry.child = make_unique<Node>(max_node_size);
        auto new_node = entry.child->insert(key, move(value));

        if (new_node) {
            assert(new_node->Entries::size() == 2);

            auto& k1 = new_node->begin()->first;
            auto& e1 = new_node->begin()->second;
            auto& e2 = (++new_node->begin())->second;

            auto j = Entries::insert(make_pair(k1, std::move(e1))).first;

            entry.child->Entries::clear();

            for (auto& e : *e2.child) {
                std::next(j)->second.child->Entries::insert(std::move(e));
            }
        }
    }
    else {
        (*this)[NodeId(string(key))] = NodeEntry{move(value), nullptr};
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

    while(!Entries::empty()) {
        if (fill_left && median-- == 0) {
            auto& e = *begin();
            left_child->inf_entry()->second.child = move(e.second.child);
            e.second.child = move(left_child);
            ret.Entries::insert(std::move(e));
            fill_left = false;
        }
        else if (fill_left) {
            left_child->Entries::insert(std::move(*begin()));
        }
        else {
            auto& ch = ret.inf_entry()->second.child;
            if (!ch) { ch = make_unique<Node>(max_node_size); }
            ch->Entries::insert(std::move(*begin()));
        }

        Entries::erase(begin());
    }

    return ret;
}

optional<Value> BTree::Node::find(const Key& key)
{
    auto i = lower_bound(key);

    assert(i != Entries::end());

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

    auto is_less = NodeIdCompare();

    for (auto& e : *this) {
        if (!e.second.child) {
            continue;
        }

        for (auto& ee : *e.second.child) {
            if (ee.first != boost::none && !is_less(ee.first, e.first)) {
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

    for (auto& e : *this) {
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
