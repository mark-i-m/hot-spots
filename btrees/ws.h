#pragma once

#include <cstdlib>
#include <unordered_map>

namespace ws {

// Is either a page or nothing.
template <typename Page>
class MaybePage {
    bool _is_page;
    const Page p;

public:
    // Construct a `MaybePage` that contains no page.
    MaybePage() : _is_page(false) {}

    // Construct a `MaybePage` that contains page `p`.
    MaybePage(Page p) : _is_page(true), p(p) {}

    // Returns true iff this `MaybePage` contains a page.
    bool is_page() const {
        return _is_page;
    }

    // Returns the page in this `MaybePage`. You should call `is_page` first to
    // check that there is a page. It is undefined behavior to call this
    // method if there is no page.
    const Page get_page() const {
        return p;
    }
};

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
    MaybePage<Page> touch(const Page p);

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
MaybePage<Page> WS<Page>::touch(const Page p) {
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
        return MaybePage<Page>(p);
    }

    return MaybePage<Page>();
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
