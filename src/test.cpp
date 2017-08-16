#include <iostream>
#include <ipfs_cache/ipfs_cache.h>
#include <signal.h>

using namespace std;

static void signal_cb(evutil_socket_t sig, short events, void * ctx)
{
    event_base* evbase = static_cast<event_base*>(ctx);
    struct timeval delay = { 0, 0 };
    event_base_loopexit(evbase, &delay);
}

static void setup_threading()
{
#ifdef EVTHREAD_USE_PTHREADS_IMPLEMENTED
    evthread_use_pthreads();
#elif defined(EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED)
    evthread_use_windows_threads();
#endif
}

int main()
{
    setup_threading();

    auto evbase = event_base_new();

    auto* signal_event = evsignal_new(evbase, SIGINT, signal_cb, evbase);

    if (!signal_event || event_add(signal_event, NULL)<0) {
        cerr << "Could not create/add a signal event!" << endl;
        return 1;
    }

    // Evbase MUST outlive IpfsCache, so putting it in a scope.
    {
        namespace ic = ipfs_cache;

        ic::IpfsCache ipfs(evbase);

        string test_data = "My test content2";

        ipfs.insert_content((uint8_t*) test_data.data(), test_data.size(), [&](string cid) {
            cout << "added " << cid << endl;

            auto q = ic::entry( "test_content2.org"
                              , ic::make_node(ic::entry("CID", cid)));

            ipfs.update_db(q, [&ipfs](auto a) {
                cout << "Updated DB at https://ipfs.io/ipns/" << ipfs.ipns_id() << endl;
            });
        });

        cout << "Press Ctrl-C to exit." << endl;
        event_base_loop(evbase, 0);
    }

    event_base_free(evbase);
    event_free(signal_event);

    return 0;
}
