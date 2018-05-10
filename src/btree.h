#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <map>
#include <iostream>
#include "namespaces.h"
#include "defer.h"

namespace ipfs_cache {

//--------------------------------------------------------------------
//                       Node
//        +--------------------------------+
//        |            NodeData            |
//        +--------------------------------|
//        | Entry1 | Entry2 | ... | EntryN |
//        +--------------------------------+
//--------------------------------------------------------------------
template<class NodeData>
class BTree {
public:
    struct Node;

    using Key   = std::string;
    using Value = std::string;
    using OnNodeChange = std::function<void(Node&)>;

public:
    // boost::none represents the last entry in a node
    // (i.e. the entry which is "bigger" than any Key)
    using NodeId = boost::optional<Key>;

    struct NodeIdCompare {
        // https://www.fluentcpp.com/2017/06/09/search-set-another-type-key/
        using is_transparent = void;
        bool operator()(const NodeId& n1, const NodeId& n2) const;
        bool operator()(const NodeId& n, const std::string& s) const;
    };

    struct Entry {
        Value value;
        std::unique_ptr<Node> child;
    };

    using Entries = std::map<NodeId, Entry, NodeIdCompare>;

    struct Node : public Entries {
    public:
        bool check_invariants() const;

        boost::optional<Node> insert(const Key&, Value, const OnNodeChange&);
        boost::optional<Value> find(const Key&);
        boost::optional<Node> split();

        size_t size() const;
        bool is_leaf() const;

        typename Entries::iterator lower_bound(const Key&);

        NodeData data;

    private:
        friend class BTree;

        Node(size_t max_node_size)
            : max_node_size(max_node_size) {}

        static std::pair<NodeId, Entry> make_inf_entry();

        std::pair<size_t,size_t> min_max_depth() const;

        typename Entries::iterator inf_entry();

    private:
        size_t max_node_size;
    };

public:
    BTree(size_t max_node_size = 512);

    boost::optional<Value> find(const Key&);
    void insert(const Key&, Value, const OnNodeChange& on_change = nullptr);

    bool check_invariants() const;

    Node* root() { return _root.get(); }

    ~BTree();

private:
     size_t _max_node_size;
     std::unique_ptr<Node> _root;
};

//--------------------------------------------------------------------
// IO
//
template<class NodeData>
inline
std::ostream& operator<<(std::ostream& os
                        , const typename BTree<NodeData>::NodeId& n) {
    if (n == boost::none) return os << "INF";
    return os << *n;
}

template<class NodeData>
inline
static std::ostream& operator<<(std::ostream& os
                               , const typename BTree<NodeData>::Node&);

template<class NodeData>
inline
static std::ostream& operator<<(std::ostream& os
                               , const typename BTree<NodeData>::Entries& es)
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

template<class NodeData>
inline
static std::ostream& operator<<( std::ostream& os
                               , const typename BTree<NodeData>::Node& n)
{
    return os << static_cast<const typename BTree<NodeData>::Entries&>(n);
}

//--------------------------------------------------------------------
// NodeIdCompare
//
template<class NodeData>
inline
bool
BTree<NodeData>::NodeIdCompare::operator()( const NodeId& n1
                                          , const NodeId& n2) const {
    if (n1 == boost::none)      return false;
    else if (n2 == boost::none) return true;
    return *n1 < *n2;
}

template<class NodeData>
inline
bool
BTree<NodeData>::NodeIdCompare::operator()( const NodeId& n
                                          , const std::string& s) const {
    if (n == boost::none) return false;
    return *n < s;
}

//--------------------------------------------------------------------
// Node
//
template<class NodeData>
inline
size_t BTree<NodeData>::Node::size() const
{
    if (Entries::empty()) return 0;
    if ((--Entries::end())->first == boost::none) return Entries::size() - 1;
    return Entries::size();
}

template<class NodeData>
inline
bool BTree<NodeData>::Node::is_leaf() const
{
    for (auto& e: static_cast<const Entries&>(*this)) {
        if (e.second.child) return false;
    }

    return true;
}

template<class NodeData>
inline
std::pair< typename BTree<NodeData>::NodeId
         , typename BTree<NodeData>::Entry>
BTree<NodeData>::Node::make_inf_entry()
{
    return std::make_pair(NodeId(), Entry{{}, nullptr});
}

template<class NodeData>
inline
typename BTree<NodeData>::Entries::iterator
BTree<NodeData>::Node::inf_entry()
{
    if (Entries::empty()) {
        return Entries::insert(make_inf_entry()).first;
    }
    auto i = --Entries::end();
    if (i->first == boost::none) return i;
    return Entries::insert(make_inf_entry()).first;
}

template<class NodeData>
inline
std::pair<size_t,size_t> BTree<NodeData>::Node::min_max_depth() const
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

template<class NodeData>
// Return iterator with key greater or equal to the `key`
inline
typename BTree<NodeData>::Entries::iterator
BTree<NodeData>::Node::lower_bound(const Key& key)
{
    auto i = Entries::lower_bound(key);
    if (i != Entries::end()) return i;
    return Entries::insert(make_inf_entry()).first;
}

template<class NodeData>
inline
boost::optional<typename BTree<NodeData>::Node>
BTree<NodeData>::Node::insert( const Key& key
                             , Value value
                             , const OnNodeChange& on_change)
{
    auto on_exit = defer([&] { if (on_change) on_change(*this); });

    if (!is_leaf()) {

        auto i = lower_bound(key);

        if (i->first == key) {
            i->second.value = move(value);
            return boost::none;
        }

        auto& entry = i->second;
        if (!entry.child) entry.child.reset(new Node(max_node_size));
        auto new_node = entry.child->insert(key, move(value), on_change);

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
        (*this)[NodeId(key)] = Entry{move(value), nullptr};
    }

    return split();
}

template<class NodeData>
inline
boost::optional<typename BTree<NodeData>::Node> BTree<NodeData>::Node::split()
{
    if (size() <= max_node_size) {
        return boost::none;
    }

    size_t median = size() / 2;
    bool fill_left = true;

    std::unique_ptr<Node> left_child(new Node(max_node_size));
    Node ret(max_node_size);

    while(!Entries::empty()) {
        if (fill_left && median-- == 0) {
            auto& e = *Entries::begin();
            left_child->inf_entry()->second.child = move(e.second.child);
            e.second.child = move(left_child);
            ret.Entries::insert(std::move(e));
            fill_left = false;
        }
        else if (fill_left) {
            left_child->Entries::insert(std::move(*Entries::begin()));
        }
        else {
            auto& ch = ret.inf_entry()->second.child;
            if (!ch) { ch.reset(new Node(max_node_size)); }
            ch->Entries::insert(std::move(*Entries::begin()));
        }

        Entries::erase(Entries::begin());
    }

    return ret;
}

template<class NodeData>
inline
boost::optional<typename BTree<NodeData>::Value>
BTree<NodeData>::Node::find(const Key& key)
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

template<class NodeData>
inline
bool BTree<NodeData>::Node::check_invariants() const {
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
template<class NodeData>
inline
BTree<NodeData>::BTree(size_t max_node_size)
    : _max_node_size(max_node_size)
{}

template<class NodeData>
inline
boost::optional<typename BTree<NodeData>::Value>
BTree<NodeData>::find(const Key& key)
{
    if (!_root) {
        return boost::none;
    }

    return _root->find(key);
}

template<class NodeData>
inline
void BTree<NodeData>::insert( const Key& key
                            , Value value
                            , const OnNodeChange& on_change)
{
    if (!_root) _root.reset(new Node(_max_node_size));

    auto n = _root->insert(key, move(value), on_change);

    if (n) {
        *_root = move(*n);
    }

    assert(_root->check_invariants());
}

template<class NodeData>
inline
bool BTree<NodeData>::check_invariants() const
{
    if (!_root) return true;
    return _root->check_invariants();
}

template<class NodeData>
inline
BTree<NodeData>::~BTree() {}

} // namespace
