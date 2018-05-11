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

//BOOST_AUTO_TEST_CASE(test_2)
//{
//    srand(time(NULL));
//
//    BTree db(nullptr, nullptr, 256);
//
//    set<string> inserted;
//
//    asio::io_service ios;
//
//    asio::spawn(ios, [&](asio::yield_context yield) {
//        sys::error_code ec;
//
//        for (int i = 0; i < 3000; ++i) {
//            int k = rand();
//            stringstream ss;
//            ss << k;
//            db.insert(ss.str(), ss.str(), yield[ec]);
//            inserted.insert(ss.str());
//            BOOST_REQUIRE(!ec);
//        }
//
//        BOOST_REQUIRE(db.check_invariants());
//
//        for (auto& key : inserted) {
//            auto val = db.find(key, yield[ec]);
//            BOOST_REQUIRE(!ec);
//            BOOST_REQUIRE(val);
//            BOOST_REQUIRE_EQUAL(key, *val);
//        }
//    });
//
//    ios.run();
//}

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

private:
    size_t next_id = 0;;
};

unsigned random_5_digit() {
    unsigned k = 0;
    while ((k % 100000) < 10000) { k = rand(); }
    return k % 100000;
}

BOOST_AUTO_TEST_CASE(test_3)
{
    //srand(time(NULL));
    //auto seed = time(NULL);
    auto seed = 1526044953;
    cerr << "seed: " << seed << endl;
    srand(seed);

    MockStorage storage;

    BTree db(storage.cat_op(), storage.add_op(), 2);

    set<string> inserted;

    asio::io_service ios;

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        string root_hash;

        for (int i = 0; i < 10; ++i) {
            int k = random_5_digit();
            stringstream ss;
            ss << k;
            db.insert(ss.str(), "v" + ss.str(), yield[ec]);
            inserted.insert(ss.str());
            BOOST_REQUIRE(!ec);

        }

        BOOST_REQUIRE(db.check_invariants());

        //for (auto& kv : storage) {
        //    cerr << ">>>>>>> " << kv.first << " " << kv.second << endl;
        //}
        for (auto& key : inserted) {
            auto val = db.find(key, yield[ec]);
            BOOST_REQUIRE(!ec);
            BOOST_REQUIRE(val);
            BOOST_REQUIRE_EQUAL("v" + key, *val);
        }

        BTree db2(storage.cat_op(), storage.add_op(), 2);

        db2.load(db.root_hash());

        //cerr << ">>>>>>> db2" << endl;
        for (auto& key : inserted) {
            auto val = db2.find(key, yield[ec]);
            if (ec) {
                cerr << ">>> " << ec.message() << endl;
            }
            BOOST_REQUIRE(!ec);
            if (!val) {
                cerr << "------------------------------------------------" << endl;
                for (auto p : storage) {
                    cerr << p.first << ": " << p.second << endl;
                }
                cerr << "------------------------------------------------" << endl;
                cerr << "db root_hash: " << db.root_hash() << endl;
                db.debug(true);
                db.find(key, yield[ec]);
                cerr << "------------------------------------------------" << endl;
                cerr << "db2 root_hash: " << db2.root_hash() << endl;
                db2.debug(true);
                db2.find(key, yield[ec]);
            }
            cerr << ">>>>>>> " << key << " " << (val ? val->c_str() : "NIL" ) << endl;
            BOOST_REQUIRE(val);
            BOOST_REQUIRE_EQUAL("v" + key, *val);
        }
    });

    ios.run();
}

BOOST_AUTO_TEST_SUITE_END()
