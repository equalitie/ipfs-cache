#pragma once

#include <ipfs_cache/query.h>
#include "query_view_struct.h"

// Query view is a C POD structure which contains the same information as the
// Query structure but (since it's C POD) can be passed to the Go language
// routines.
namespace ipfs_cache {

//// Debuging:
//void print_query_view(query_view* dv, std::string pad) {
//    using std::cout;
//    using std::endl;
//
//    cout << pad << "str_size:  " << dv->str_size << endl;
//    cout << pad << "str:       " << (dv->str ? dv->str : "<nil>") << endl;
//    cout << pad << "child_cnt: " << dv->child_count << endl;
//    for (size_t i = 0; i < dv->child_count; i++) {
//        cout << pad << "  child " << i << endl;
//        print_query_view(&dv->childs[i], pad + "    ");
//    }
//}

// Creates the query_view structure according to `query`. The result is allocated
// on the stack so it's not returned but instead it's applied to `f`.
template<class F>
void with_query_view(const Query& query, const F& f) {
    enum class Type { string, entry, node };

    struct E {
        Type  type;
        const void* query;

        E() {}
        explicit E(const Query* in) {
            if (auto s = boost::get<std::string>(in)) {
                type = Type::string;
                query = s;
            }
            else if (auto e = boost::get<entry>(in)) {
                type = Type::entry;
                query = e;
            }
            else if (auto n = boost::get<node>(in)) {
                type = Type::node;
                query = n;
            }
            else { assert(0); }
        }

        E(const std::pair<const std::string, Query>* e) {
            type = Type::entry;
            query = e;
        }

        std::string* as_string() {
            return type == Type::string ? (std::string*) query : nullptr;
        }
        entry* as_entry() {
            return type == Type::entry ? (entry*) query : nullptr;
        }
        node* as_node() {
            return type == Type::node ? (node*) query : nullptr;
        }
    };

    struct D {
        E cur;
        bool is_set;
        query_view* view;
        D* next;
    };

    D* root   = (D*) alloca(sizeof(D));

    D* d      = root;
    d->cur    = E(&query);
    d->is_set = false;
    d->view   = (query_view*) alloca(sizeof(query_view));
    d->next   = nullptr;

    while (d) {
        if (d->is_set) {
            d = d->next;
            continue;
        }

        d->is_set = true;

        if (auto s = d->cur.as_string()) {
            d->view->str_size    = s->size();
            d->view->str         = s->data();
            d->view->child_count = 0;
            d->view->childs      = nullptr;
        }
        else if (auto e = d->cur.as_entry()) {
            auto ch = (D*) alloca(sizeof(D));
            ch->cur    = E(&e->second);
            ch->is_set = false;
            ch->view   = (query_view*) alloca(sizeof(query_view));
            ch->next   = d;

            d->view->str_size    = e->first.size();
            d->view->str         = e->first.data();
            d->view->child_count = 1;
            d->view->childs      = ch->view;

            d = ch;
        }
        else if (auto n = d->cur.as_node()) {
            auto child_views = (query_view*) alloca(n->size() * sizeof(query_view));

            size_t k = 0;
            D* first = nullptr;
            D* prev  = nullptr;
            for (auto& i : *n) {
                auto ch = (D*) alloca(sizeof(D));
                ch->cur    = E(&i);
                ch->is_set = false;
                ch->view   = &child_views[k++];
                ch->next   = d;

                if (prev) prev->next = ch;
                if (!first) first = ch;

                prev = ch;
            }
            
            d->view->str_size    = 0;
            d->view->str         = nullptr;
            d->view->child_count = n->size();
            d->view->childs      = child_views;

            d = first;
        }
    }

    f(root->view);
}

} // ipfs_cache namespace
