#pragma once

#include <map>

// TODO: Does Boost.Beast not support ULR query decoding?

namespace _parse_vars_detail {
    using boost::beast::string_view;

    inline string_view make_view(const char* b, const char* e)
    {
        return string_view(b, e - b);
    }

    inline
    const char* find(char c, const char* b, const char* e)
    {
        for (auto i = b; i != e; ++i) if (*i == c) return i;
        return e;
    }

    inline
    std::pair<string_view, string_view>
    parse_keyval(const char* b, const char* e)
    {
        auto p = find('=', b, e);
        auto v = (p == e) ? make_view(e, e) : make_view(p + 1, e);
        return std::make_pair(make_view(b, p), v);
    }
}

inline
std::map< boost::beast::string_view
        , boost::beast::string_view>
parse_vars(const std::string& vars)
{
    using namespace _parse_vars_detail;
    using namespace std;
    using boost::beast::string_view;

    map<string_view, string_view> ret;

    const char* b = vars.c_str();
    const char* e = vars.c_str() + vars.size();

    for (;;) {
        auto p = find('&', b, e);
        auto kv = parse_keyval(b, p);
        ret.insert(kv);
        if (p == e) break;
        b = p + 1;
    }

    return ret;
}
