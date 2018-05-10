#include "btree.h"

using namespace ipfs_cache;

using Key   = BTree::Key;
using Value = BTree::Value;
using Hash  = BTree::Hash;
using Node  = BTree::Node;

//--------------------------------------------------------------------
//                       Node
//        +--------------------------------|
//        | Entry1 | Entry2 | ... | EntryN |
//        +--------------------------------+
//--------------------------------------------------------------------

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

struct BTree::Node : public Entries {
public:
    bool check_invariants() const;

    boost::optional<Node> insert(const Key&, Value);
    boost::optional<Value> find(const Key&) const;
    boost::optional<Node> split();

    size_t size() const;
    bool is_leaf() const;

    typename Entries::iterator find_or_create_lower_bound(const Key&);

private:
    friend class BTree;

    Node(size_t max_node_size)
        : max_node_size(max_node_size) {}

    static std::pair<NodeId, Entry> make_inf_entry();

    std::pair<size_t,size_t> min_max_depth() const;

    void insert_node(Node n);

    typename Entries::iterator inf_entry();

private:
    size_t max_node_size;
};

//--------------------------------------------------------------------
// IO
//
std::ostream& operator<<(std::ostream& os, const NodeId& n) {
    if (n == boost::none) return os << "INF";
    return os << *n;
}

static std::ostream& operator<<(std::ostream& os
                               , const BTree::Node&);

static std::ostream& operator<<(std::ostream& os, const Entries& es)
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

static std::ostream& operator<<( std::ostream& os
                               , const BTree::Node& n)
{
    return os << static_cast<const Entries&>(n);
}

//Json node_to_json(const Tree::Node& n)
//{
//    Json json;
//
//    for (auto& e : n) {
//        const char* k = e.first ? e.first->c_str() : "";
//
//        json[k]["value"] = e.second.value;
//
//        if (e.second.child) {
//            auto& ipfs_hash = e.second.child->data.ipfs_hash;
//            assert(!ipfs_hash.empty());
//            json[k]["child"] = ipfs_hash;
//        }
//    }
//
//    return json;
//}

//--------------------------------------------------------------------
// NodeIdCompare
//
bool
NodeIdCompare::operator()(const NodeId& n1, const NodeId& n2) const {
    if (n1 == boost::none)      return false;
    else if (n2 == boost::none) return true;
    return *n1 < *n2;
}

bool
NodeIdCompare::operator()(const NodeId& n, const std::string& s) const {
    if (n == boost::none) return false;
    return *n < s;
}

//--------------------------------------------------------------------
// Node
//
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

std::pair<NodeId, Entry>
BTree::Node::make_inf_entry()
{
    return std::make_pair(NodeId(), Entry{{}, nullptr});
}

Entries::iterator BTree::Node::inf_entry()
{
    if (Entries::empty()) {
        return Entries::insert(make_inf_entry()).first;
    }
    auto i = --Entries::end();
    if (i->first == boost::none) return i;
    return Entries::insert(make_inf_entry()).first;
}

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
Entries::iterator
BTree::Node::find_or_create_lower_bound(const Key& key)
{
    auto i = Entries::lower_bound(key);
    if (i != Entries::end()) return i;
    return Entries::insert(make_inf_entry()).first;
}

void BTree::Node::insert_node(Node n)
{
    assert(n.Entries::size() == 2);
    
    auto& k1 = n.begin()->first;
    auto& e1 = n.begin()->second;
    auto& e2 = (++n.begin())->second;
    
    auto j = Entries::insert(make_pair(k1, std::move(e1))).first;
    
    assert(std::next(j) != Entries::end());
    auto& entry = std::next(j)->second;
    entry.child->Entries::clear();
    
    for (auto& e : *e2.child) {
        std::next(j)->second.child->Entries::insert(std::move(e));
    }
}

boost::optional<BTree::Node>
BTree::Node::insert(const Key& key, Value value)
{
    //auto on_exit = defer([&] { if (on_change) on_change(*this); });

    if (!is_leaf()) {

        auto i = find_or_create_lower_bound(key);

        if (i->first == key) {
            i->second.value = move(value);
            return boost::none;
        }

        auto& entry = i->second;
        if (!entry.child) entry.child.reset(new Node(max_node_size));

        auto new_node = entry.child->insert(key, move(value));

        if (new_node) {
            insert_node(std::move(*new_node));
        }
    }
    else {
        (*this)[NodeId(key)] = Entry{move(value), nullptr};
    }

    return split();
}

boost::optional<BTree::Node> BTree::Node::split()
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

boost::optional<BTree::Value> BTree::Node::find(const Key& key) const
{
    auto i = Entries::lower_bound(key);

    if (i == Entries::end()) {
        return boost::none;
    }

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

//--------------------------------------------------------------------
// BTree
//
BTree::BTree(CatOp cat_op, AddOp add_op, size_t max_node_size)
    : _max_node_size(max_node_size)
    , _cat_op(std::move(cat_op))
    , _add_op(std::move(add_op))
{}

boost::optional<BTree::Value>
BTree::find(const Key& key) const
{
    if (!_root) {
        return boost::none;
    }

    return _root->find(key);
}

void BTree::insert(const Key& key, Value value)
{
    if (!_root) _root.reset(new Node(_max_node_size));

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

//void BTree::update_ipfs(Tree::Node* n, asio::yield_context yield)
//{
//    if (!n) return;
//    if (!n->data.ipfs_hash.empty()) return;
//
//    auto d = _was_destroyed;
//
//    for (auto& e : *n) {
//        auto& ch = e.second.child;
//        if (!ch) continue;
//        sys::error_code ec;
//        update_ipfs(ch.get(), yield[ec]);
//        if (ec) return or_throw(yield, ec);
//    }
//
//    Json json = node_to_json(*n);
//
//    sys::error_code ec;
//    auto new_ipfs_hash = _add_op(json.dump(), yield[ec]);
//
//    if (!ec && *d) ec = asio::error::operation_aborted;
//    if (ec) return or_throw(yield, ec);
//
//    n->data.ipfs_hash = move(new_ipfs_hash);
//}
