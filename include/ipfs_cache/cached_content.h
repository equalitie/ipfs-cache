#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>

namespace ipfs_cache {

struct CachedContent {
    // Data time stamp, not a date/time on errors.
    boost::posix_time::ptime ts;
    // Cached data.
    std::string data;
};

} // ipfs_cache namespace
