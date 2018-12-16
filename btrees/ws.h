#ifndef _BTREE_WS_H_
#define _BTREE_WS_H_

#include "util.h"

#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <atomic>
#include <functional>
#include <mutex>

namespace btree_hybrid {

// Tracks stats for ranges of keys that have been touched and chooses ranges to
// evict from the cache.
//
// We do very little synchronization in this data structure, relying instead on
// the fact that the B-tree hybrid implementation uses the `big_lock` to control
// when certain operations can happen.
template <typename K, size_t N>
class WS {
    // We implement an LRU replacement policy. Each range has a unique position
    // in the LRU list.
    //
    // Using a linked list makes concurrency hard, so instead we implement it
    // as follows:
    // - There is a global counter `next`, which starts at 1.
    // - Each element in the list is mapped to a counter value. The lowest
    //   counter value is the oldest.
    // - To make element `i` the MRU, increment `next` and place the old value
    //   in `counters[i]`.
    // - To find the LRU, scan `counters` for the lowest element.
    // - To evict an element, set `counters[i]` to 0 and remove the low key
    //   from `lru_map`.
    // - To insert an element, grab the writer lock. Scan `counters` for a 0
    //   entry. If none is found, evict the LRU. Then insert the new value.
    util::RangeMap<K, size_t> lru_map;
    K low_keys[N];
    K high_keys[N];
    std::atomic_uint64_t counters[N];
    std::atomic_uint64_t next;

    // Indicates whether the cache needs to be "purged" (a range evicted).
    bool next_should_purge = false;

    // A mutex lock for small critical sections in the case of insertions.
    mutable std::mutex lock;

    // Set range i to the MRU if it is still in the map.
    //
    // NOTE: caller should already be holding the lock.
    void set_mru(size_t i);

    // Returns true if the range [kl, kh) has an overlap with another range.
    // We assume that no range is completely subsumed by another range.
    bool weird_overlaps(K kl, K kh);

public:

    // Construct a WS with at most `N` pages.
    WS();

    // Inform the WS that the given key k in range [kl, kh) has been touched.
    // Returns true if the key/range is hot and should be cached.
    bool touch(const K& kl, const K& kh, const K& k);

    // Remove the given range [kl, kh) from the WS. This should only be called
    // on ranges returned from `purge_range` and only after they have been
    // removed from the cache.
    void remove(const K& kl, const K& kh);

    // Returns true if the cache requires a purge (because it is full).
    bool needs_purge() const;

    // Returns the range to purge. This should only be called if `needs_purge`
    // is true.
    std::pair<K, K> purge_range() const;
};

////////////////////////////////////////////////////////////////////////////////
// Implementations
////////////////////////////////////////////////////////////////////////////////

template <typename K, size_t N>
void WS<K, N>::set_mru(size_t i) {
    // NOTE: the caller must hold a reader or writer lock. Otherwise, the `i`th
    // entry could be evicted under our feet!

    auto mru = next.fetch_add(1);

    // NOTE: There is a slight chance that we overwrite a more recent counter
    // value, but without grabbing a lock, this is the best we can do, so I
    // will take the chance.

    counters[i].store(mru);
}

template <typename K, size_t N>
WS<K, N>::WS() {
    next = 1;
    std::fill(counters, &counters[N], 0);
}

template <typename K, size_t N>
bool WS<K, N>::weird_overlaps(K kl, K kh) {
    const auto maybe_low = lru_map.find(kl);
    const auto maybe_high = lru_map.find(kh);
    return maybe_low || maybe_high;
}

template <typename K, size_t N>
bool WS<K, N>::touch(const K& kl, const K& kh, const K& k) {
    auto maybe = lru_map.find(k);
    if (maybe) {
        // If the given key is already in a hot range, make that range the MRU.
        set_mru(**maybe);
        return true;
    } else {
        // If the given key is not in a hot range, we atomically insert it into
        // the WS. This involves a brief critical section.
        //
        // To simplify life, we just reject a range if
        // a) the WS is already full... OR
        // b) the range has a weird overlap with another range.
        lock.lock();
        if (lru_map.size() == N) { // full
            next_should_purge = true;
            lock.unlock();
            return false;
        }
        if (weird_overlaps(kl, kh)) {
            lock.unlock();
            return false;
        }

        // Insert

        // Find a free slot and the LRU.
        size_t lru = 0;
        size_t lru_counter = (size_t)-1;

        for (size_t i = 0; i < N; ++i) {
            auto c = counters[i].load();

            if (c < lru_counter) {
                lru_counter = c;
                lru = i;
            }
        }

        // Do the insertion...
        assert(lru_counter == 0);

        low_keys[lru] = kl;
        high_keys[lru] = kh;
        lru_map.insert(kl, kh, lru);

        // Make MRU.
        set_mru(lru);

        lock.unlock();
        return true;
    }
}

template <typename K, size_t N>
void WS<K, N>::remove(const K& kl, const K&) {
    // NOTE: no lock needed because this can only be called while holding the big_lock(w)
    const auto idx = lru_map.remove(kl);
    counters[idx] = 0;
    low_keys[idx] = 0xDEADBEEF;
    high_keys[idx] = 0xDEADBEEF;
    next_should_purge = false;
    assert(lru_map.size() < N);
}

template <typename K, size_t N>
bool WS<K, N>::needs_purge() const {
    // Returns true if the WS is full and should be purged.
    return lru_map.size() == N && next_should_purge;
}

template <typename K, size_t N>
std::pair<K, K> WS<K, N>::purge_range() const {
    // NOTE: no lock needed because this can only be called while holding the big_lock(w)

    // Find a free slot and the LRU.
    size_t lru = 0;
    size_t lru_counter = (size_t)-1;

    K evicted_low;
    K evicted_high;

    for (size_t i = 0; i < N; ++i) {
        auto c = counters[i].load();

        if (c < lru_counter) {
            lru_counter = c;
            lru = i;
        }
    }

    assert(lru_counter > 0);
    assert(lru_map.size() == N);

    evicted_low = low_keys[lru];
    evicted_high = high_keys[lru];

    return {evicted_low, evicted_high};
}

} // namespace btree_hybrid

#endif
