#include <ipfs_cache/c_interface.h>

#include <ipfs_cache/client.h>
#include <ipfs_cache/injector.h>

#include <string.h> // memmove
#include <string>
#include <iostream>

using namespace std;
using namespace ipfs_cache;

//--------------------------------------------------------------------
// Client
//--------------------------------------------------------------------
extern "C"
ipfs_cache_client* ipfs_cache_client_new(struct event_base* evbase,
                                         const char* ipns,
                                         const char* path_to_repo)
{
    Client* c = nullptr;
    
    try {
        c = new Client(evbase, ipns, path_to_repo);
    }
    catch(const exception& e) {
        cerr << "Error in ipfs_cache_client_new: " << e.what() << endl;
    }

    return (ipfs_cache_client*) c;
}

extern "C"
void ipfs_cache_client_delete(ipfs_cache_client* cc)
{
    delete (Client*) cc;
}

extern "C"
void ipfs_cache_client_get_content(
        ipfs_cache_client* cc,
        const char* url,
        void (*cb)(const char* /* content */, size_t /* content_size */, void* /* user_data */),
        void* user_data)
{
    auto c = (Client*) cc;

    c->get_content(url, [cb, user_data](vector<char> content) {
            if (!cb) return;
            cb(content.data(), content.size(), user_data);
    });
}

//--------------------------------------------------------------------
// Injector
//--------------------------------------------------------------------
extern "C"
ipfs_cache_injector* ipfs_cache_injector_new(struct event_base* evbase,
                                             const char* path_to_repo)
{
    Injector* i = nullptr;

    try {
        i = new Injector(evbase, path_to_repo);
    }
    catch(const exception& e) {
        cerr << "Error in ipfs_cache_injector_new: " << e.what() << endl;
    }

    return (ipfs_cache_injector*) i;
}

extern "C"
void ipfs_cache_injector_delete(ipfs_cache_injector* i)
{
    delete (Injector*) i;
}

extern "C"
const char* ipfs_cache_injector_ipns_id(ipfs_cache_injector* c_inj)
{
    auto inj = (Injector*) c_inj;
    auto str = inj->ipns_id();
    char* c_str = (char*) malloc(str.size() + 1);
    memmove(c_str, str.data(), str.size());
    c_str[str.size()] = '\0';
    return c_str;
}

extern "C"
void ipfs_cache_injector_insert_content(
        ipfs_cache_injector* c_inj,
        const char* key,
        const char* content,
        size_t content_size,
        void(*cb)(const char* /* ipfs id */, void* /* user_data */),
        void* user_data)
{
    auto inj = (Injector*) c_inj;

    // TODO: The copying in copy constructor of strings can be optimized away.
    inj->insert_content( key
                       , vector<char>(content, content + content_size)
                       , [cb, user_data](string ipfs_id) {
        if (!cb) return;
        cb(ipfs_id.c_str(), user_data);
    });
}
