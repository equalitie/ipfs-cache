#ifndef IPFS_CACHE_C_INTERFACE_H__
#define IPFS_CACHE_C_INTERFACE_H__

#include <event2/event.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {} ipfs_cache_client;
typedef struct {} ipfs_cache_injector;

//--------------------------------------------------------------------
// Client
//--------------------------------------------------------------------
ipfs_cache_client* ipfs_cache_client_new(
        struct event_base*,
        const char* ipns,
        const char* path_to_repo);

void ipfs_cache_client_delete(ipfs_cache_client*);

void ipfs_cache_client_get_content(
        ipfs_cache_client*,
        const char* url,
        void (*cb)( const char* /* content */
                  , size_t      /* content_size */
                  , void*       /* user_data */),
        void* user_data);

//--------------------------------------------------------------------
// Injector
//--------------------------------------------------------------------
ipfs_cache_injector* ipfs_cache_injector_new(struct event_base*, const char* path_to_repo);
void ipfs_cache_injector_delete(ipfs_cache_injector*);

// NOTE: The returned string must be explicly freed!
const char* ipfs_cache_injector_ipns_id(ipfs_cache_injector*);

void ipfs_cache_injector_insert_content(
        ipfs_cache_injector*,
        const char* key,
        const char* content,
        size_t content_size,
        void(*cb)(const char* /* ipfs id */, void* /* user_data */),
        void* user_data);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ifndef IPFS_CACHE_C_INTERFACE_H__
