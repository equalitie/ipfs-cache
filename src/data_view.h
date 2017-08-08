#pragma once

#include <ipfs_cache/data.h>
#include "data_view_struct.h"

// Data view is a C POD structure which contains the same information as the
// Data structure but (since it's C POD) can be passed to the Go language
// routines.
namespace ipfs_cache {

//// Debuging:
//void print_data_view(data_view* dv, std::string pad) {
//    using std::cout;
//    using std::endl;
//
//    cout << pad << "str_size:  " << dv->str_size << endl;
//    cout << pad << "str:       " << (dv->str ? dv->str : "<nil>") << endl;
//    cout << pad << "child_cnt: " << dv->child_count << endl;
//    for (size_t i = 0; i < dv->child_count; i++) {
//        cout << pad << "  child " << i << endl;
//        print_data_view(&dv->childs[i], pad + "    ");
//    }
//}

// Creates the data_view structure according to `data`. The result is allocated
// on the stack so it's not returned but instead it's applied to `f`.
template<class F>
void with_data_view(const Data& data, const F& f) {
    enum class Type { string, entry, node };

    struct E {
        Type  type;
        const void* data;

        E() {}
        explicit E(const Data* in) {
            if (auto s = boost::get<std::string>(in)) {
                type = Type::string;
                data = s;
            }
            else if (auto e = boost::get<entry>(in)) {
                type = Type::entry;
                data = e;
            }
            else if (auto n = boost::get<node>(in)) {
                type = Type::node;
                data = n;
            }
            else { assert(0); }
        }

        E(const std::pair<const std::string, Data>* e) {
            type = Type::entry;
            data = e;
        }

        std::string* as_string() {
            return type == Type::string ? (std::string*) data : nullptr;
        }
        entry* as_entry() {
            return type == Type::entry ? (entry*) data : nullptr;
        }
        node* as_node() {
            return type == Type::node ? (node*) data : nullptr;
        }
    };

    struct D {
        E cur;
        bool is_set;
        data_view* view;
        D* next;
    };

    D* root   = (D*) alloca(sizeof(D));

    D* d      = root;
    d->cur    = E(&data);
    d->is_set = false;
    d->view   = (data_view*) alloca(sizeof(data_view));
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
            ch->view   = (data_view*) alloca(sizeof(data_view));
            ch->next   = d;

            d->view->str_size    = e->first.size();
            d->view->str         = e->first.data();
            d->view->child_count = 1;
            d->view->childs      = ch->view;

            d = ch;
        }
        else if (auto n = d->cur.as_node()) {
            auto child_views = (data_view*) alloca(n->size() * sizeof(data_view));

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
