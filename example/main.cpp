#include <iostream>
#include <ipfs_cache/ipfs_cache.h>
#include <signal.h>
#include <event2/thread.h>

#include "options.h"

using namespace std;

static void quit(event_base* evbase)
{
    struct timeval delay = { 0, 0 };
    event_base_loopexit(evbase, &delay);
}

static void signal_cb(evutil_socket_t sig, short events, void * ctx)
{
    event_base* evbase = static_cast<event_base*>(ctx);
    quit(evbase);
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
    Options options;

    try {
        options.parse(argc, argv);
    }
    catch (const std::exception& e) {
        cerr << "Error parsing options: " << e.what() << "\n" << endl;
        cerr << options << endl;
        return 1;
    }

    if (options.help()) {
        cout << options << endl;
        return 0;
    }

    setup_threading();

    auto evbase = event_base_new();

    auto* signal_event = evsignal_new(evbase, SIGINT, signal_cb, evbase);

    if (!signal_event || event_add(signal_event, NULL)<0) {
        cerr << "Could not create/add a signal event!" << endl;
        return 1;
    }

    // Evbase MUST outlive IpfsCache, so putting it in a scope.
    try {
        cout << "Starting event loop, press Ctrl-C to exit." << endl;

        namespace ic = ipfs_cache;

        ic::IpfsCache ipfs(evbase, options.ipns(), options.repo());

        if (options.inject()) {
            cout << "Inserting content..." << endl;
            ipfs.insert_content(
                    (uint8_t*) options.value().data(),
                    options.value().size(),
                    [&](string ipfs_id) {
                        cout << "Updating database..." << endl;
                        ipfs.update_db(options.key(), ipfs_id, [&]{
                                cout << "Database " << ipfs.ipns_id() << " updated" << endl;
                            });
                    });
        }

        if (options.fetch()) {
            cout << "Fetching..." << endl;
            ipfs.query_db(options.key(), [&](string value) {
                        cout << "Value:" << value << endl;
                    });
        }

        event_base_loop(evbase, 0);
    }
    catch (const exception& e) {
        cerr << "Exception " << e.what() << endl;
    }

    event_base_free(evbase);
    event_free(signal_event);

    return 0;
}
