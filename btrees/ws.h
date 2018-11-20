#ifndef _BTREE_WS_H_
#define _BTREE_WS_H_

#include "util.h"

#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <pthread.h>
#include <atomic>
#include <functional>

namespace btree_hybrid {

// Tracks stats for ranges of pages that have been touched and chooses ranges
// to evict.
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

    // LOCKING RULES
    // - You must grab a lock to read or write the `lru_map` or `counters`.
    // - You must grab a _write_ lock to insert or evict a range.
    mutable pthread_rwlock_t lock;

    // Set range i to the MRU if it is still in the map.
    //
    // NOTE: caller should already be holding the lock.
    void set_mru(size_t i);

    // NOTE: Grabs a lock. Beware of deadlock.
    void print() {
        pthread_rwlock_wrlock(&lock);
        for (size_t i = 0; i < N; ++i) {
            std::cout << "[" << low_keys[i] << ", " << high_keys[i] << ") " << counters[i] << std::endl;
        }
        std::cout << std::endl;
        pthread_rwlock_unlock(&lock);
    }

public:

    typedef std::function<void(K, K)> purge_fn;

    // Construct a WS with at most `N` pages.
    WS();

    ~WS();

    // Update stats by registering a touch for key `k`. If a range should be
    // evicted, `f` is invoked on that range while holding an exclusive lock.
    //
    // Note that the _CALLER_ should verify that `k` is in a range that is
    // _already_ in the WS. If it is not, use the other `touch`.
    void touch(const K k, purge_fn f);

    // Update stats by registering a touch for key `k` in range `[kl, kh)`
    // which should _not_ already be in the WS. If a range should be
    // evicted, `f` is invoked on that range while holding an exclusive lock.
    //
    // Note that the _CALLER_ should verify that `[kl, kh)` is NOT _already_ in
    // the WS. If it is, use the other `touch`.
    void touch(const K kl, const K kh, const K k, purge_fn f);

    // Returns true iff the given key k is in a range in the WS.
    bool is_hot(const K& k);
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

    int ret = pthread_rwlock_init(&lock, NULL);
    assert(ret == 0);
}

template <typename K, size_t N>
WS<K, N>::~WS() {
    int ret = pthread_rwlock_destroy(&lock);
    assert(ret == 0);
}

template <typename K, size_t N>
void WS<K, N>::touch(const K k, purge_fn) {
    pthread_rwlock_rdlock(&lock);

    auto maybe = lru_map.find(k);
    assert(maybe);

    // Set to MRU
    set_mru(**maybe);

    pthread_rwlock_unlock(&lock);
}

template <typename K, size_t N>
void WS<K, N>::touch(const K kl, const K kh, const K k, purge_fn f) {
    assert(kl <= k && k < kh);

    pthread_rwlock_wrlock(&lock);

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

    // lru_counter == 0 => free

    // None free. Need to evict LRU.
    if (lru_counter > 0) {
        counters[lru].store(0);
        evicted_low = low_keys[lru];
        evicted_high = high_keys[lru];
        lru_map.remove(evicted_low, evicted_high);
    }

    // Insert new.
    low_keys[lru] = kl;
    high_keys[lru] = kh;
    lru_map.insert(kl, kh, lru);

    // Make MRU.
    set_mru(lru);

    if (lru_counter > 0) {
        f(evicted_low, evicted_high);
    }

    pthread_rwlock_unlock(&lock);
}

template <typename K, size_t N>
bool WS<K, N>::is_hot(const K& k) {
    pthread_rwlock_rdlock(&lock);
    auto maybe = lru_map.find(k);
    pthread_rwlock_unlock(&lock);

    return maybe;
}

} // namespace btree_hybrid

#endif
