#include <iostream>
#include <ipfs_cache/injector.h>
#include <ipfs_cache/client.h>
#include <signal.h>
#include <event2/thread.h>
#include <boost/program_options.hpp>

using namespace std;

static void signal_cb(evutil_socket_t sig, short events, void * ctx)
{
    event_base_loopexit(static_cast<event_base*>(ctx), nullptr);
}

static void setup_threading()
{
#ifdef EVTHREAD_USE_PTHREADS_IMPLEMENTED
    evthread_use_pthreads();
#elif defined(EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED)
    evthread_use_windows_threads();
#else
#   error No support for threading
#endif
}

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

    /*
     * We need this because ipfs_cache "pushes" events into our event loop from
     * other threads.
     */
    setup_threading();

    auto evbase = event_base_new();

    auto* signal_event = evsignal_new(evbase, SIGINT, signal_cb, evbase);

    if (!signal_event || event_add(signal_event, NULL)<0) {
        cerr << "Could not create/add a signal event!" << endl;
        return 1;
    }

    try {
        cout << "Starting event loop, press Ctrl-C to exit." << endl;

        ipfs_cache::Client client(evbase, ipns, repo);

        cout << "Fetching..." << endl;
        client.get_content(key, [&](string value) {
                    cout << "Value:" << value << endl;
                    event_base_loopexit(evbase, NULL);
                });

        event_base_loop(evbase, 0);
    }
    catch (const exception& e) {
        cerr << "Exception " << e.what() << endl;
    }

    event_base_free(evbase);
    event_free(signal_event);

    return 0;
}
