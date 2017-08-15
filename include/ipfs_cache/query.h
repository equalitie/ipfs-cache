#pragma once

#include <map>
#include <boost/variant.hpp>

namespace ipfs_cache {

using Query = boost::make_recursive_variant
                < std::string
                , std::pair<const std::string, boost::recursive_variant_>
                , std::map<std::string, boost::recursive_variant_>
                >::type;

using entry = std::pair<const std::string, Query>;
using node = std::map<std::string, Query>;

namespace impl {
    inline void make_node(node&) {}
    
    template<class T, class... Ts>
    inline void make_node(node& ret, T t, Ts... ts)
    {
        ret[std::move(t.first)] = std::move(t.second);
        make_node(ret, std::forward<Ts>(ts)...);
    }
} // impl namespace

template<class... Ts> inline node make_node(Ts... ts)
{
    node ret;
    impl::make_node(ret, std::forward<Ts>(ts)...);
    return ret;
}

} // ipfs_cache namespace
