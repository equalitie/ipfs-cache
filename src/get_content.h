#pragma once

#include <ipfs_cache/cached_content.h>

namespace ipfs_cache {

template<class Db>
inline
void get_content( Db& db
                , std::string url
                , std::function<void(sys::error_code, CachedContent)> cb)
{
    auto& ios = db.backend().get_io_service();

    sys::error_code ec;

    CacheEntry entry = db.query(url, ec);

    if (!ec && entry.ts.is_not_a_date_time()) {
        ec = asio::error::not_found;
    }

    if (ec) {
        return ios.post([cb = std::move(cb), ec] { cb(ec, CachedContent()); });
    }

    db.backend().cat( entry.content_hash
                    , entry.content_size
                    , [cb = std::move(cb), ts = entry.ts]
                      (sys::error_code ecc, std::string s) {
            if (ecc) {
                return cb(ecc, CachedContent());
            }

            CachedContent cont({ts, s});
            cb(ecc, std::move(cont));
        });
}

template<class Db>
inline
CachedContent get_content(Db& db, std::string url, asio::yield_context yield)
{
    using handler_type = typename asio::handler_type
                           < asio::yield_context
                           , void(sys::error_code, CachedContent)>::type;

    handler_type handler(yield);
    asio::async_result<handler_type> result(handler);

    get_content(db, std::move(url), handler);

    return result.get();
}

} // ipfs_cache namespace
