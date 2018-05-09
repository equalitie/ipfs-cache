#define BOOST_TEST_MODULE db_tree
#include <boost/test/included/unit_test.hpp>
#include <boost/optional.hpp>

#include <btree.h>
#include <namespaces.h>
#include <iostream>

BOOST_AUTO_TEST_SUITE(db_tree)

using namespace std;
using namespace ipfs_cache;

using boost::optional;

BOOST_AUTO_TEST_CASE(test_1)
{
    BTree db;

    db.insert("key", "value");
    optional<string> v = db.find("key");
    BOOST_REQUIRE(v);
    BOOST_REQUIRE_EQUAL(*v, "value");
}

BOOST_AUTO_TEST_CASE(test_2)
{
    srand(time(NULL));

    BTree db(256);

    set<string> inserted;

    for (int i = 0; i < 10000; ++i) {
        int k = rand();
        stringstream ss;
        ss << k;
        db.insert(ss.str(), ss.str());
    }

    BOOST_REQUIRE(db.check_invariants());

    for (auto& key : inserted) {
        auto val = db.find(key);
        BOOST_REQUIRE(val);
        BOOST_REQUIRE_EQUAL(key, *val);
    }
}

BOOST_AUTO_TEST_SUITE_END()
