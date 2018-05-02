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

BOOST_AUTO_TEST_CASE(test_1)
{
    auto cat_op = [](const string& key, asio::yield_context yield) {
        return key + ":val"; 
    };

    DbTree db(cat_op);

    asio::io_service ios;

    asio::spawn(ios, [&] (asio::yield_context yield) {
        sys::error_code ec;

        string r = db.find("key", yield[ec]);

        cout << ">>> " << ec.message() << endl;
    });

    ios.run();
}

BOOST_AUTO_TEST_SUITE_END()
