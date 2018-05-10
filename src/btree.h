#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <map>
#include <iostream>
#include "namespaces.h"

namespace ipfs_cache {

class BTree {
public:
    using Key   = std::string;
    using Value = std::string;

public:
    // boost::none represents the last entry in a node
    // (i.e. the entry which is "bigger" than any Key)
    using NodeId = boost::optional<Key>;

    struct Node;

    struct NodeIdCompare {
        // https://www.fluentcpp.com/2017/06/09/search-set-another-type-key/
        using is_transparent = void;
        bool operator()(const NodeId& n1, const NodeId& n2) const;
        bool operator()(const NodeId& n, const std::string& s) const;
    };

    struct NodeEntry {
        Value value;
        std::unique_ptr<Node> child;
    };

    using Entries = std::map<NodeId, NodeEntry, NodeIdCompare>;

    struct Node : public Entries {
    public:
        Node(size_t max_node_size)
            : max_node_size(max_node_size) {}

        bool check_invariants() const;

        boost::optional<Node> insert(const Key&, Value);
        boost::optional<Value> find(const Key&);
        boost::optional<Node> split();

        size_t size() const;
        std::pair<size_t,size_t> min_max_depth() const;
        bool is_leaf() const;

        iterator inf_entry();
        iterator lower_bound(const Key&);

    private:
        static std::pair<NodeId, NodeEntry> make_inf_entry();

    private:
        size_t max_node_size;
    };

public:
    BTree(size_t max_node_size = 512);

    boost::optional<Value> find(const Key&);
    void insert(const Key&, Value);

    bool check_invariants() const;

    ~BTree();

private:
     size_t _max_node_size;
     std::unique_ptr<Node> _root;
};

//--------------------------------------------------------------------
// IO
//
inline
std::ostream& operator<<(std::ostream& os, const BTree::NodeId& n) {
    if (n == boost::none) return os << "INF";
    return os << *n;
}

inline
static std::ostream& operator<<(std::ostream& os, const BTree::Node&);

inline
static std::ostream& operator<<(std::ostream& os, const BTree::Entries& es)
{
    os << "{";
    for (auto i = es.begin(); i != es.end(); ++i) {
        os << i->first;
        if (i->second.child) {
            os << ":" << *i->second.child;
        }

        if (std::next(i) != es.end()) os << " ";
    }
    return os << "}";
}

inline
static std::ostream& operator<<(std::ostream& os, const BTree::Node& n)
{
    return os << static_cast<const BTree::Entries&>(n);
}

//--------------------------------------------------------------------
// NodeIdCompare
//
inline
bool
BTree::NodeIdCompare::operator()(const NodeId& n1, const NodeId& n2) const {
    if (n1 == boost::none)      return false;
    else if (n2 == boost::none) return true;
    return *n1 < *n2;
}

inline
bool
BTree::NodeIdCompare::operator()(const NodeId& n, const std::string& s) const {
    if (n == boost::none) return false;
    return *n < s;
}

//--------------------------------------------------------------------
// Node
//
inline
size_t BTree::Node::size() const
{
    if (Entries::empty()) return 0;
    if ((--Entries::end())->first == boost::none) return Entries::size() - 1;
    return Entries::size();
}

inline
bool BTree::Node::is_leaf() const
{
    for (auto& e: static_cast<const Entries&>(*this)) {
        if (e.second.child) return false;
    }

    return true;
}

inline
std::pair<BTree::NodeId, BTree::NodeEntry> BTree::Node::make_inf_entry()
{
    return std::make_pair(NodeId(), NodeEntry{{}, nullptr});
}

inline
BTree::Entries::iterator BTree::Node::inf_entry()
{
    if (Entries::empty()) {
        return Entries::insert(make_inf_entry()).first;
    }
    auto i = --Entries::end();
    if (i->first == boost::none) return i;
    return Entries::insert(make_inf_entry()).first;
}

inline
std::pair<size_t,size_t> BTree::Node::min_max_depth() const
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

    return std::make_pair(min, max);
}

// Return iterator with key greater or equal to the `key`
inline
BTree::Entries::iterator BTree::Node::lower_bound(const Key& key)
{
    auto i = Entries::lower_bound(key);
    if (i != Entries::end()) return i;
    return Entries::insert(make_inf_entry()).first;
}

inline
boost::optional<BTree::Node>
BTree::Node::insert(const Key& key, Value value)
{
    if (!is_leaf()) {

        auto i = lower_bound(key);

        if (i->first == key) {
            i->second.value = move(value);
            return boost::none;
        }

        auto& entry = i->second;
        if (!entry.child) entry.child = std::make_unique<Node>(max_node_size);
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
        (*this)[NodeId(key)] = NodeEntry{move(value), nullptr};
    }

    return split();
}

inline
boost::optional<BTree::Node> BTree::Node::split()
{
    if (size() <= max_node_size) {
        return boost::none;
    }

    size_t median = size() / 2;
    bool fill_left = true;

    auto left_child = std::make_unique<Node>(max_node_size);
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
            if (!ch) { ch = std::make_unique<Node>(max_node_size); }
            ch->Entries::insert(std::move(*begin()));
        }

        Entries::erase(begin());
    }

    return ret;
}

inline
boost::optional<BTree::Value> BTree::Node::find(const Key& key)
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

inline
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

//--------------------------------------------------------------------
// BTree
//
inline
BTree::BTree(size_t max_node_size)
    : _max_node_size(max_node_size)
{}

inline
boost::optional<BTree::Value> BTree::find(const Key& key)
{
    if (!_root) {
        return boost::none;
    }

    return _root->find(key);
}

inline
void BTree::insert(const Key& key, Value value)
{
    if (!_root) _root = std::make_unique<Node>(_max_node_size);

    auto n = _root->insert(key, move(value));

    if (n) {
        *_root = move(*n);
    }

    assert(_root->check_invariants());
}

inline
bool BTree::check_invariants() const
{
    if (!_root) return true;
    return _root->check_invariants();
}

inline
BTree::~BTree() {}

} // namespace
