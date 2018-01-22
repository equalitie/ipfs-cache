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

    cout << "Starting event loop, press Ctrl-C to exit." << endl;

    asio::spawn(ios, [&](asio::yield_context yield) {
            ipfs_cache::Client client(ios, ipns, repo);

            try {
                cout << "Waiting for DB update..." << endl;
                client.wait_for_db_update(yield);

                cout << "Fetching..." << endl;
                ipfs_cache::CachedContent value = client.get_content(key, yield);

                cout << "Date: " << value.date << endl
                     << "Value: " << value.data.dump() << endl;
            }
            catch (const exception& e) {
                cerr << "Error: " << e.what() << endl;
            }
        });

    ios.run();

    return 0;
}
