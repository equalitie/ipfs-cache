#define BOOST_TEST_MODULE db_tree
#include <boost/test/included/unit_test.hpp>
#include <boost/optional.hpp>

#include <db_tree.h>
#include <or_throw.h>
#include <namespaces.h>
#include <iostream>

BOOST_AUTO_TEST_SUITE(db_tree)

using namespace std;
using namespace ipfs_cache;

struct MockIpfs {
    DbTree::CatOp cat_operation() {
        return [this](const DbTree::Hash& hash, asio::yield_context yield) {
            auto i = memory.find(hash);
            if (i == memory.end()) {
                return or_throw<DbTree::Value>(yield, asio::error::not_found);
            }
            return i->second; 
        };
    }

    DbTree::AddOp add_operation() {
        return [this](const DbTree::Value& data, asio::yield_context yield) {
            stringstream ss;
            ss << memory.size();
            string key = ss.str();
            memory[key] = data;
            return key; 
        };
    }

    map<DbTree::Hash, DbTree::Value> memory;
};

BOOST_AUTO_TEST_CASE(test_1)
{
    MockIpfs mock_ipfs;

    DbTree db(mock_ipfs.cat_operation(), mock_ipfs.add_operation());

    asio::io_service ios;

    asio::spawn(ios, [&] (asio::yield_context yield) {
        sys::error_code ec;
        db.insert("key", "value", yield[ec]);
        BOOST_REQUIRE(!ec);
        string v = db.find("key", yield[ec]);
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE_EQUAL(v, "value");
    });

    ios.run();
}

BOOST_AUTO_TEST_CASE(test_2)
{
    MockIpfs mock_ipfs;

    DbTree db(mock_ipfs.cat_operation(), mock_ipfs.add_operation());

    asio::io_service ios;

    set<string> inserted;

    asio::spawn(ios, [&] (asio::yield_context yield) {
        sys::error_code ec;

        for (int i = 0; i < 100000; ++i) {
            int k = rand();
            stringstream ss;
            ss << k;
            db.insert(ss.str(), ss.str(), yield[ec]);
            BOOST_REQUIRE(!ec);
        }

        for (auto& key : inserted) {
            auto val = db.find(key, yield[ec]);
            BOOST_REQUIRE(!ec);
            BOOST_REQUIRE_EQUAL(key, val);
        }
    });

    ios.run();
}

BOOST_AUTO_TEST_SUITE_END()
