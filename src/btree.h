#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <set>
#include "namespaces.h"

namespace ipfs_cache {

class BTree {
public:
    using Key   = std::string;
    using Value = std::string;

public:
    struct Node;
    struct NodeEntry;

public:
    BTree(size_t max_node_size = 512);

    boost::optional<Value> find(const Key&);
    void insert(const Key&, Value);

    bool check_invariants() const;

    void print(std::ostream&) const;

    ~BTree();

private:
     size_t _max_node_size;
     std::unique_ptr<Node> _root;
};

} // namespace
