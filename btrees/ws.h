#ifndef _BTREE_WS_H_
#define _BTREE_WS_H_

#include "util.h"

#include <cstdlib>
#include <unordered_map>

namespace ws {

// Tracks stats for ranges of pages that have been touched and chooses ranges
// to evict.
template <typename K>
class WS {
    size_t n;
    util::RangeMap<K, uint64_t> stats;

public:
    // Construct a WS with at most `n` pages.
    WS(size_t n);

    // Update stats by registering a touch for key `k`. If a range should be
    // evicted, returns that range in a `Maybe`.
    //
    // Note that the _CALLER_ should verify that `k` is in a range that is
    // _already_ in the WS. If it is not, use the other `touch`.
    util::maybe::Maybe<std::pair<K, K>> touch(const K k);

    // Update stats by registering a touch for key `k` in range `[kl, kh)`
    // which may not already be in the WS. If a range should be evicted,
    // returns that range in a `Maybe`.
    util::maybe::Maybe<std::pair<K, K>> touch(const K k, const K kl, const K kh);

    // Returns true iff the given key k is in a range in the WS.
    bool is_hot(const K& k);

    // Clears all stats.
    void clear();
};

////////////////////////////////////////////////////////////////////////////////
// Implementations
////////////////////////////////////////////////////////////////////////////////

template <typename K>
WS<K>::WS(size_t n) : n(n) {}

template <typename K>
util::maybe::Maybe<std::pair<K, K>> WS<K>::touch(const K k) {
    auto maybe = stats.lookup(k);
    assert(maybe.is_value());
    maybe.get_value() += 1;

    return util::maybe::Maybe<std::pair<K, K>>();
}

template <typename K>
util::maybe::Maybe<std::pair<K, K>> WS<K>::touch(const K k, const K kl, const K kh) {
    auto maybe = stats.lookup(k);

    // If the key is not even in the map, insert it.
    if (!maybe.is_value()) {
        stats.insert_range(kl, kh);
        stats.insert_key(k);

        // Do we need to evict something?
        if (stats.size() > n) {
            // TODO: choose something... probably LRU
            return util::maybe::Maybe<std::pair<K, K>>();
        }
    }

    // Otherwise, update the stats.
    else {
        maybe.get_value() += 1;
    }

    // Maybe to evict
    return util::maybe::Maybe<std::pair<K, K>>();
}

template <typename K>
bool WS<K>::is_hot(const K& k) {
    return stats.lookup(k).is_value();
}

template <typename K>
void WS<K>::clear() {
    stats.clear();
}

} // namespace ws

#endif
