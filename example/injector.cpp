#include <iostream>
#include <boost/program_options.hpp>
#include <ipfs_cache/injector.h>
#include <ipfs_cache/client.h>
#include <signal.h>
#include <event2/thread.h>

#include "injector_server.h"

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
        ("repo", po::value<std::string>()->default_value("./repo"),
         "Path to the IPFS repository")
        ("port,p", po::value<uint16_t>()->default_value(0),
         "Port the server will listen on (use 0 for random)")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")) {
        cout << desc << endl;
        return 0;
    }

    string repo   = vm["repo"].as<string>();
    uint16_t port = vm["port"].as<uint16_t>();

    /*
     * We need this because ipfs_cache "pushes" events into our event loop from
     * other threads.
     */
    setup_threading();

    /*
     * Tell libevent we want to exit on Ctrl-C.
     */
    auto evbase = event_base_new();

    auto* signal_event = evsignal_new(evbase, SIGINT, signal_cb, evbase);

    if (!signal_event || event_add(signal_event, NULL)<0) {
        cerr << "Could not create/add a signal event!" << endl;
        return 1;
    }

    /*
     * Create the injector and the server and start the main loop.
     */
    try {
        ipfs_cache::Injector injector(evbase, repo);
        InjectorServer server(evbase, port, injector);

        cout << "Listening on port " << server.listening_port() << endl;
        cout << "IPNS of this database is " << injector.ipns_id() << endl;
        cout << "Starting event loop, press Ctrl-C to exit." << endl;

        event_base_loop(evbase, 0);
    }
    catch (const exception& e) {
        cerr << "Exception " << e.what() << endl;
    }

    /*
     * Cleanup.
     */
    event_base_free(evbase);
    event_free(signal_event);

    return 0;
}
