#pragma once

namespace ipfs_cache {

template<class F>
class Defer {
public:
    Defer(F f) : _f(std::move(f)) {}
    ~Defer() { _f(); }

private:
    F _f;
};

template<class F>
Defer<F> defer(F f) { return Defer<F>(std::move(f)); }

} // ipfs_cache namespace
