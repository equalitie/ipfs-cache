#pragma once

#include <string>
#include <boost/system/error_code.hpp>

// Inspired by this post
// http://breese.github.io/2016/06/18/unifying-error-codes.html

namespace ipfs_cache { namespace error {

    enum error_t {
        key_not_found,
    };
    
    struct ipfs_cache_category : public boost::system::error_category
    {
        const char* name() const noexcept override
        {
            return "ipfs_cache_errors";
        }
    
        std::string message(int e) const
        {
            switch (e) {
                case error::key_not_found:
                    return "key not found";
                default:
                    return "unknown ipfs_cache error";
            }
        }
    };
    
    inline
    boost::system::error_code
    make_error_code(::ipfs_cache::error::error_t e)
    {
        static ipfs_cache_category c;
        return boost::system::error_code(static_cast<int>(e), c);
    }

}} // ipfs_cache::error namespace

namespace boost { namespace system {

    template<>
    struct is_error_code_enum<::ipfs_cache::error::error_t>
        : public std::true_type {};


}} // boost::system namespace
