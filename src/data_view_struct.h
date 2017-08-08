#ifndef IPFS_CACHE_DATA_VIEW_STRUCT_H
#define IPFS_CACHE_DATA_VIEW_STRUCT_H

struct data_view {
    size_t      str_size;
    const char* str;

    size_t            child_count;
    struct data_view* childs;
};

#endif // ifndef IPFS_CACHE_DATA_VIEW_STRUCT_H
