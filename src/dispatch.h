#pragma once

namespace ipfs_cache {

// A helper function for sending lambdas of type void() to be executed
// in the event_base loop.
template<class F>
static void dispatch(event_base* evbase, F f) {
    struct DispatchCtx {
        F callback;
        struct event* ev;
    
        static void cb(evutil_socket_t, short, void *param) {
            auto ctx = static_cast<DispatchCtx*>(param);
            auto cb = std::move(ctx->callback);
            event_free(ctx->ev);
            delete ctx;
            cb();
        }
    };

    auto ctx = new DispatchCtx{std::move(f)};
    struct event* ev = event_new(evbase, -1, EV_PERSIST, DispatchCtx::cb, ctx);
    ctx->ev = ev;
    event_active(ev, 0, 0);
}

} // ipfs_cache namespace
