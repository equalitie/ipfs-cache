#pragma once

#include <assert.h>

// A helper function for sending lambdas of type void() to be executed
// in the event_base loop.
template<class F>
static void dispatch(event_base* evbase, F f) {
    // TODO(peterj) Check out the event_active function to do this (might be
    // more efficient).

    using namespace std;

    struct DispatchCtx {
        F callback;
        struct event* ev;
    
        static void cb(evutil_socket_t, short, void *param) {
            auto ctx = static_cast<DispatchCtx*>(param);
            auto cb = move(ctx->callback);
            event_free(ctx->ev);
            delete ctx;
            cb();
        }
    };

    struct timeval zero_sec;

    zero_sec.tv_sec = 0;
    zero_sec.tv_usec = 0;

    auto ctx = new DispatchCtx{move(f)};

    struct event* ev = event_new(evbase, -1, EV_PERSIST, DispatchCtx::cb, ctx);

    ctx->ev = ev;

    int add_result = event_add(ev, &zero_sec);
    assert(add_result == 0);
}

