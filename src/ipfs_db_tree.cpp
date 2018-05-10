#include <json.hpp>
#include "ipfs_db_tree.h"
#include "or_throw.h"

using Json = nlohmann::json;

using namespace ipfs_cache;
using namespace std;

IpfsDbTree::IpfsDbTree(CatOp cat, AddOp add)
    : _btree(256)
    , _cat_op(std::move(cat))
    , _add_op(std::move(add))
    , _was_destroyed(std::make_shared<bool>(false))
{}

Json node_to_json(const IpfsDbTree::Tree::Node& n)
{
    Json json;

    for (auto& e : n) {
        const char* k = e.first ? e.first->c_str() : "";

        json[k]["value"] = e.second.value;

        if (e.second.child) {
            auto& ipfs_hash = e.second.child->data.ipfs_hash;
            assert(!ipfs_hash.empty());
            json[k]["child"] = ipfs_hash;
        }
    }

    return json;
}

void IpfsDbTree::update_ipfs(Tree::Node* n, asio::yield_context yield)
{
    if (!n) return;
    if (!n->data.ipfs_hash.empty()) return;

    auto d = _was_destroyed;

    for (auto& e : *n) {
        auto& ch = e.second.child;
        if (!ch) continue;
        sys::error_code ec;
        update_ipfs(ch.get(), yield[ec]);
        if (ec) return or_throw(yield, ec);
    }

    Json json = node_to_json(*n);

    sys::error_code ec;
    auto new_ipfs_hash = _add_op(json.dump(), yield[ec]);

    if (!ec && *d) ec = asio::error::operation_aborted;
    if (ec) return or_throw(yield, ec);

    n->data.ipfs_hash = move(new_ipfs_hash);
}

void IpfsDbTree::insert(const Key& key, Value value, asio::yield_context yield)
{
    _btree.insert(key, std::move(value), [] (Tree::Node& n) {
            n.data.ipfs_hash.clear();
        });

    update_ipfs(_btree.root(), yield);
}

boost::optional<IpfsDbTree::Value>
IpfsDbTree::find(const Key& key, asio::yield_context)
{
    return _btree.find(key);
}

IpfsDbTree::~IpfsDbTree()
{
    *_was_destroyed = true;
}



