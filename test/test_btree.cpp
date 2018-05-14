#define BOOST_TEST_MODULE db_tree
#include <boost/test/included/unit_test.hpp>
#include <boost/optional.hpp>

#include <btree.h>
#include <namespaces.h>
#include <iostream>

#include "or_throw.h"

BOOST_AUTO_TEST_SUITE(db_tree)

using namespace std;
using namespace ipfs_cache;

using boost::optional;

BOOST_AUTO_TEST_CASE(test_1)
{
    BTree db;
    
    asio::io_service ios;

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;
        db.insert("key", "value", yield[ec]);
        BOOST_REQUIRE(!ec);
        optional<string> v = db.find("key", yield[ec]);
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE(v);
        BOOST_REQUIRE_EQUAL(*v, "value");
    });

    ios.run();
}

BOOST_AUTO_TEST_CASE(test_2)
{
    srand(time(NULL));

    BTree db(nullptr, nullptr, nullptr, 256);

    set<string> inserted;

    asio::io_service ios;

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        for (int i = 0; i < 3000; ++i) {
            int k = rand();
            stringstream ss;
            ss << k;
            db.insert(ss.str(), ss.str(), yield[ec]);
            inserted.insert(ss.str());
            BOOST_REQUIRE(!ec);
        }

        BOOST_REQUIRE(db.check_invariants());

        for (auto& key : inserted) {
            auto val = db.find(key, yield[ec]);
            BOOST_REQUIRE(!ec);
            BOOST_REQUIRE(val);
            BOOST_REQUIRE_EQUAL(key, *val);
        }
    });

    ios.run();
}

struct MockStorage : public std::map<BTree::Hash, BTree::Value> {
    using Map = std::map<BTree::Hash, BTree::Value>;

    BTree::CatOp cat_op() {
        return [this] (BTree::Hash hash, asio::yield_context yield) {
            auto i = Map::find(hash);
            if (i == Map::end()) {
                return or_throw<BTree::Value>(yield, asio::error::not_found);
            }
            return i->second;
        };
    }

    BTree::AddOp add_op() {
        return [this] (BTree::Value value , asio::yield_context yield) {
            std::stringstream ss;
            ss << next_id++;
            auto id = ss.str();
            Map::operator[](id) = std::move(value);
            return id;
        };
    }

    BTree::UnpinOp unpin_op() {
        return [this] (const BTree::Hash& h, asio::yield_context) {
            Map::erase(h);
        };
    }

private:
    size_t next_id = 0;;
};

// It's easier to debug with fixed length digit string because of sorting (with
// strings: "20" > "100")
unsigned random_5_digit() {
    unsigned k = 0;
    while ((k % 100000) < 10000) { k = rand(); }
    return k % 100000;
}

BOOST_AUTO_TEST_CASE(test_3)
{
    srand(time(NULL));

    MockStorage storage;

    BTree db(storage.cat_op(), storage.add_op(), storage.unpin_op(), 2);

    set<string> inserted;

    asio::io_service ios;

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        string root_hash;

        for (int i = 0; i < 100; ++i) {
            int k = random_5_digit();
            stringstream ss;
            ss << k;
            db.insert(ss.str(), "v" + ss.str(), yield[ec]);
            inserted.insert(ss.str());
            BOOST_REQUIRE(!ec);
        }

        BOOST_REQUIRE(db.check_invariants());

        BOOST_REQUIRE_EQUAL(storage.size(), db.local_node_count());

        for (auto& key : inserted) {
            auto val = db.find(key, yield[ec]);
            BOOST_REQUIRE(!ec);
            BOOST_REQUIRE(val);
            BOOST_REQUIRE_EQUAL("v" + key, *val);
        }

        BTree db2(storage.cat_op(), storage.add_op(), storage.unpin_op(), 2);

        db2.load(db.root_hash(), yield[ec]);
        BOOST_REQUIRE(!ec);

        for (auto& key : inserted) {
            auto val = db2.find(key, yield[ec]);
            BOOST_REQUIRE(!ec);
            BOOST_REQUIRE(val);
            BOOST_REQUIRE_EQUAL("v" + key, *val);
        }
    });

    ios.run();
}

BOOST_AUTO_TEST_SUITE_END()
