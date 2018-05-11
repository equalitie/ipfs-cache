#include "btree.h"
#include "or_throw.h"
#include <json.hpp>
#include <iostream>

using namespace ipfs_cache;

using Key   = BTree::Key;
using Value = BTree::Value;
using Hash  = BTree::Hash;
using Node  = BTree::Node;
using AddOp = BTree::AddOp;

using Json  = nlohmann::json;

using std::cout;
using std::endl;
//--------------------------------------------------------------------
//                       Node
//        +--------------------------------+
//        | Entry1 | Entry2 | ... | EntryN |
//        +--------------------------------+
//--------------------------------------------------------------------

// boost::none represents the last entry in a node' entries
// (i.e. the entry with elements "bigger" than any Key)
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
    std::string child_hash;
};

using Entries = std::map<NodeId, Entry, NodeIdCompare>;

struct BTree::Node : public Entries {
public:
    bool check_invariants() const;
    void assert_every_node_has_hash() const;

    boost::optional<Node> insert(Key, Value);
    boost::optional<Value> find(const Key&, const CatOp&, asio::yield_context);
    boost::optional<Node> split();

    size_t size() const;
    bool is_leaf() const;

    typename Entries::iterator find_or_create_lower_bound(const Key&);

    Hash store(const AddOp&, asio::yield_context);
    void restore(Hash, const CatOp&, asio::yield_context);

private:
    friend class BTree;

    Node(BTree* tree) : _tree(tree) {}

    static std::pair<NodeId, Entry> make_inf_entry();

    std::pair<size_t,size_t> min_max_depth() const;

    void insert_node(Node n);

    typename Entries::iterator inf_entry();

    bool debug() const { return _tree->_debug; }

private:
    BTree* _tree;
};

//--------------------------------------------------------------------
// IO
//
std::ostream& operator<<(std::ostream& os, const NodeId& n) {
    if (n == boost::none) return os << "INF";
    return os << *n;
}

static std::ostream& operator<<(std::ostream& os, const Node&);

static std::ostream& operator<<(std::ostream& os, const Entries& es)
{
    os << "{";
    for (auto i = es.begin(); i != es.end(); ++i) {
        os << i->first;
        os << ":" << i->second.child_hash << ":";
        if (i->second.child) {
            os << *i->second.child;
        }
        else {
            os << "NUL";
        }

        if (std::next(i) != es.end()) os << " ";
    }
    return os << "}";
}

static std::ostream& operator<<(std::ostream& os, const Node& n)
{
    return os << static_cast<const Entries&>(n);
}

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
size_t Node::size() const
{
    if (Entries::empty()) return 0;
    if ((--Entries::end())->first == boost::none) return Entries::size() - 1;
    return Entries::size();
}

bool Node::is_leaf() const
{
    for (auto& e: static_cast<const Entries&>(*this)) {
        if (e.second.child) return false;
    }

    return true;
}

std::pair<NodeId, Entry>
Node::make_inf_entry()
{
    return std::make_pair(NodeId(), Entry{{}, nullptr});
}

Entries::iterator Node::inf_entry()
{
    if (Entries::empty()) {
        return Entries::insert(make_inf_entry()).first;
    }
    auto i = --Entries::end();
    if (i->first == boost::none) return i;
    return Entries::insert(make_inf_entry()).first;
}

std::pair<size_t,size_t> Node::min_max_depth() const
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
Node::find_or_create_lower_bound(const Key& key)
{
    auto i = Entries::lower_bound(key);
    if (i != Entries::end()) return i;
    return Entries::insert(make_inf_entry()).first;
}

void Node::insert_node(Node n)
{
    assert(n.Entries::size() == 2);
    
    auto& k1 = n.begin()->first;
    auto& e1 = n.begin()->second;
    auto& e2 = (++n.begin())->second;
    
    auto j = Entries::insert(make_pair(std::move(k1), std::move(e1))).first;
    
    assert(std::next(j) != Entries::end());
    auto& entry = std::next(j)->second;
    entry.child->Entries::clear();
    
    for (auto& e : *e2.child) {
        std::next(j)->second.child->Entries::insert(std::move(e));
    }
}

boost::optional<Node>
Node::insert(Key key, Value value)
{
    if (!is_leaf()) {
        auto i = find_or_create_lower_bound(key);

        if (i->first == key) {
            i->second.value = move(value);
            return boost::none;
        }

        auto& entry = i->second;
        if (!entry.child) entry.child.reset(new Node(_tree));

        auto new_node = entry.child->insert(move(key), move(value));

        // This will force hash recalculation when storing.
        entry.child_hash.clear();

        if (new_node) {
            insert_node(std::move(*new_node));
        }
    }
    else {
        Entries::insert(make_pair(move(key), Entry{move(value), nullptr}));
    }

    return split();
}

boost::optional<Node> Node::split()
{
    if (size() <= _tree->_max_node_size) {
        return boost::none;
    }

    size_t median = size() / 2;
    bool fill_left = true;

    std::unique_ptr<Node> left_child(new Node(_tree));
    Node ret(_tree);

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
            if (!ch) { ch.reset(new Node(_tree)); }
            ch->Entries::insert(std::move(*Entries::begin()));
        }

        Entries::erase(Entries::begin());
    }

    return ret;
}

boost::optional<Value> Node::find( const Key& key
                                 , const CatOp& cat_op
                                 , asio::yield_context yield)
{
    if (debug()) cout << "Node::find key:" << key << " " << *this << endl;
    auto i = Entries::lower_bound(key);

    if (i == Entries::end()) {
        if (debug()) cout << "  lower bound not found" << endl;
        return boost::none;
    }

    auto& e = i->second;

    if (i->first == key) {
        if (debug()) cout << "  found" << endl;

        return e.value;
    }
    else {
        if (debug()) cout << "  descending" << endl;

        if (!e.child) {
            if (debug()) cout << "    no child" << endl;

            if (e.child_hash.empty()) {
                if (debug()) cout << "    no hash" << endl;
                return boost::none;
            }

            return _tree->find( e.child_hash
                              , e.child
                              , key
                              , cat_op
                              , yield);
        }

        return e.child->find(key, cat_op, yield);
    }
}

void Node::assert_every_node_has_hash() const
{
    for (auto& kv : *this) {
        auto& e = kv.second;
        if (e.child) {
            if (e.child_hash.empty()) {
                cout << "child " << *e.child << " doesn't have a hash" << endl;
            }
            assert(!e.child_hash.empty());
        }
    }

    for (auto& kv : *this) {
        auto& e = kv.second;
        if (e.child) e.child->assert_every_node_has_hash();
    }
}

bool Node::check_invariants() const {
    if (size() > _tree->_max_node_size) {
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

Hash Node::store(const AddOp& add_op, asio::yield_context yield)
{
    cout << this << " 000 " << *this << endl;
    assert(add_op);

    auto d = _tree->_was_destroyed;

    Json json;

    for (auto& p : *this) {
        const char* k = p.first ? p.first->c_str() : "";
        cout << this << "   \"" << k << "\"" << endl;
        auto &e = p.second;

        if (p.first) {
            json[k]["value"] = e.value;
        }

        if (!e.child_hash.empty()) {
            json[k]["child"] = e.child_hash;
        }
        else if (e.child) {
            sys::error_code ec;

            auto child_hash = e.child->store(add_op, yield[ec]);

            if (!ec && *d) ec = asio::error::operation_aborted;
            if (ec) return or_throw<Json>(yield, ec);

            e.child_hash = std::move(child_hash);

            json[k]["child"] = e.child_hash;
        }
    }

    cout << this << " 111 " << *this << endl;
    assert_every_node_has_hash();
    cout << this << " 222" << endl;;
    return add_op(json.dump(), yield);
}

void Node::restore(Hash hash, const CatOp& cat_op, asio::yield_context yield)
{
    auto d = _tree->_was_destroyed;

    sys::error_code ec;
    std::string data = cat_op(hash, yield[ec]);

    if (!ec && *d) ec = asio::error::operation_aborted;
    if (ec) return or_throw(yield, ec);

    cout << "----------- " << __LINE__ << endl;
    try {
        auto json = Json::parse(data);

        cout << "----------- " << __LINE__ << endl;
        Entries::clear();

        for (auto i = json.begin(); i != json.end(); ++i) {
            cout << "----------- " << __LINE__ << endl;
            Json v = i.value();

            std::string child_hash;

            cout << "----------- " << __LINE__ << endl;
            auto child_i = v.find("child");

            cout << "----------- " << __LINE__ << endl;
            if (child_i != v.end()) {
                cout << "----------- " << __LINE__ << endl;
                child_hash = move(child_i.value());
                cout << "----------- " << __LINE__ << endl;
            }

            boost::optional<std::string> key;
           
            if (!i.key().empty()) {
                cout << "----------- " << __LINE__ << endl;
                key = i.key();
                cout << "----------- " << __LINE__ << endl;
            }

            cout << "----------- " << __LINE__ << v["value"] << endl;
            std::string value;

            if (v["value"].is_string()) {
                value = move(v["value"]);
            }
            Entries::insert(make_pair( key
                                     , Entry{ move(value)
                                            , nullptr
                                            , move(child_hash) }));
            cout << "----------- " << __LINE__ << endl;
        }
    }
    catch(const std::exception& e) {
        cout << ">>>>>>>>>>>>>>>>> " << e.what() << endl;
        return or_throw(yield, asio::error::bad_descriptor);
    }
}

//--------------------------------------------------------------------
// BTree
//
BTree::BTree(CatOp cat_op, AddOp add_op, size_t _max_node_size)
    : _max_node_size(_max_node_size)
    , _cat_op(std::move(cat_op))
    , _add_op(std::move(add_op))
    , _was_destroyed(std::make_shared<bool>(false))
{}

boost::optional<Value>
BTree::find( const Hash& hash
           , std::unique_ptr<Node>& n
           , const Key& key
           , const CatOp& cat_op
           , asio::yield_context yield)
{
    if (_debug) cout << "BTree::find hash:" << hash << " key:" << key << endl;

    if (!n) {
        if (hash.empty()) {
            return boost::none;
        }
        else {
            n.reset(new Node(this));

            auto d = _was_destroyed;

            sys::error_code ec;
            n->restore(hash, cat_op, yield[ec]);

            if (!ec && *d) ec = asio::error::operation_aborted;
            if (ec) return or_throw(yield, ec, boost::none);
        }
    }

    return n->find(key, cat_op, yield);
}

boost::optional<Value>
BTree::find(const Key& key, asio::yield_context yield)
{
    auto i = _insert_buffer.find(key);

    if (i != _insert_buffer.end()) {
        return i->second;
    }

    return find(_root_hash, _root, key, CatOp(_cat_op), yield);
}

void BTree::raw_insert(Key key, Value value)
{
    if (!_root) _root.reset(new Node(this));

    auto n = _root->insert(key, move(value));

    if (n) {
        *_root = move(*n);
    }

    assert(_root->check_invariants());
}

void BTree::insert(Key key, Value value, asio::yield_context yield)
{
    if (_is_inserting) {
        _insert_buffer.insert(std::make_pair(key, std::move(value)));
        return;
    }

    _is_inserting = true;
    auto on_exit = defer([&] { _is_inserting = false; });

    _insert_buffer.insert(make_pair(std::move(key), std::move(value)));

    auto d = _was_destroyed;

    sys::error_code ec;

    while (!_insert_buffer.empty() && !ec)
    {
        auto buf = std::move(_insert_buffer);

        for (auto& kv : buf) {
            raw_insert(std::move(kv.first), std::move(kv.second));
        }

        if (_root && _add_op) {
            // We must use a copy of _add_op to handle the case where `this`
            // get's destroyed while the store operation is running.
            Hash root_hash = _root->store(AddOp(_add_op), yield[ec]);
            if (!ec && *d) ec = asio::error::operation_aborted;
            if (!ec) {
                _root_hash = std::move(root_hash);
                _root->assert_every_node_has_hash();
            }
        }
        else {
            _root_hash.clear();
        }
    }

    return or_throw(yield, ec);
}

void BTree::load(Hash hash) {
    _root = nullptr;
    _insert_buffer.clear();
    _root_hash = move(hash);
}

bool BTree::check_invariants() const
{
    if (!_root) return true;
    return _root->check_invariants();
}

BTree::~BTree() {
    *_was_destroyed = true;
}

