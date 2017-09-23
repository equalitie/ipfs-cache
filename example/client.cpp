#include <iostream>
#include <ipfs_cache/injector.h>
#include <ipfs_cache/client.h>
#include <signal.h>
#include <boost/program_options.hpp>
#include <boost/asio/io_service.hpp>

using namespace std;
namespace asio = boost::asio;

int main(int argc, const char** argv)
{
    /*
     * Parse command line options.
     */
    namespace po = boost::program_options;

    po::options_description desc("Options");

    desc.add_options()
        ("help", "Produce this help message")
        ("repo", po::value<string>()->default_value("./repo"),
         "Path to the IPFS repository")
        ("ipns", po::value<string>(), "IPNS of the database")
        ("key", po::value<string>(), "Key to retrieve")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")) {
        cout << desc << endl;
        return 0;
    }

    string repo = vm["repo"].as<string>();

    if (!vm.count("ipns")) {
        cerr << "The 'ipns' parameter must be set" << endl;
        cerr << desc << endl;
        return 1;
    }

    string ipns = vm["ipns"].as<string>();

    if (!vm.count("key")) {
        cerr << "The 'key' parameter must be set" << endl;
        cerr << desc << endl;
        return 1;
    }

    string key = vm["key"].as<string>();

    asio::io_service ios;

    try {
        cout << "Starting event loop, press Ctrl-C to exit." << endl;

        ipfs_cache::Client client(ios, ipns, repo);

        cout << "Fetching..." << endl;
        client.get_content(key, [&](vector<char> value) {
                    cout << "Value:" << string(value.begin(), value.end()) << endl;
                });

        ios.run();
    }
    catch (const exception& e) {
        cerr << "Exception " << e.what() << endl;
    }

    return 0;
}
