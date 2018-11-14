#pragma once

#include "util.h"

#include <cstdlib>
#include <unordered_map>

namespace ws {

// Tracks stats for pages that have been touched and chooses pages to evict.
template <typename Page>
class WS {
    size_t k;
    std::unordered_map<Page, uint64_t> stats;

public:
    // Construct a WS with at most `k` pages.
    WS(size_t k);

    // Update stats by registering a touch for page `p`. If a page should be
    // evicted, returns that page in a `MaybePage`. Otherwise, returns an empty
    // `MaybePage`.
    util::Maybe<Page> touch(const Page p);

    // Returns true iff the given page is in the WS.
    bool is_hot(const Page& p);

    // Clears all stats.
    void clear();
};

////////////////////////////////////////////////////////////////////////////////
// Implementations
////////////////////////////////////////////////////////////////////////////////

template <typename Page>
WS<Page>::WS(size_t k) : k(k) {}

// TODO: need to actually do some sort of least used...
template <typename Page>
util::Maybe<Page> WS<Page>::touch(const Page p) {
    if (stats.count(p) == 0) {
        stats.insert(std::move(p), 0);
    } else {
        stats.find(p).second += 1;
    }

    // Return something for the sake of testing
    if (stats.size() > k) {
        auto it = stats.begin();
        Page p = *it;
        stats.erase(it);
        return util::Maybe<Page>(p);
    }

    return util::Maybe<Page>();
}

template <typename Page>
bool WS<Page>::is_hot(const Page& p) {
    return stats.count(p) > 0;
}

template <typename Page>
void WS<Page>::clear() {
    stats.clear();
}

} // namespace ws
