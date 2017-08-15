#ifndef IPFS_CACHE_QUERY_VIEW_STRUCT_H
#define IPFS_CACHE_QUERY_VIEW_STRUCT_H

// This would normally be defined in query_view.h but since
// it's included in the Go code, it must not contain any
// C++ language features.
struct query_view {
    size_t      str_size;
    const char* str;

    size_t            child_count;
    struct query_view* childs;
};

#endif // ifndef IPFS_CACHE_QUERY_VIEW_STRUCT_H
