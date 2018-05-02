#include "db_tree.h"
#include "or_throw.h"

using namespace std;
using namespace ipfs_cache;

DbTree::DbTree(CatOp cat_op)
    : _cat_op(move(cat_op))
{}

DbTree::Value DbTree::find(const Key& key, asio::yield_context yield)
{
    auto i = _sites.find(key);

    if (i == _sites.end()) {
        return or_throw<Value>(yield, asio::error::not_found);
    }

    return i->second;
}
