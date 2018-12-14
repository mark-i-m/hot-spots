#ifndef _BTREE_WS_H_
#define _BTREE_WS_H_

#include "util.h"

#include <iostream>
#include <cstdlib>
#include <unordered_map>
//#include <pthread.h>
#include <atomic>
#include <functional>
#include <mutex>

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
    bool next_should_purge = false;

    // LOCKING RULES
    // - You must grab a lock to read or write the `lru_map` or `counters`.
    // - You must grab a _write_ lock to insert or evict a range.
    //mutable pthread_rwlock_t lock;

    mutable std::mutex lock;

    // Set range i to the MRU if it is still in the map.
    //
    // NOTE: caller should already be holding the lock.
    void set_mru(size_t i);

    /*
    // NOTE: Grabs a lock. Beware of deadlock.
    void print() const {
        pthread_rwlock_wrlock(&lock);
        for (size_t i = 0; i < N; ++i) {
            std::cout << "[" << low_keys[i] << ", " << high_keys[i] << ") " << counters[i] << std::endl;
        }
        std::cout << std::endl;
        pthread_rwlock_unlock(&lock);
    }

    // NOTE: debugging. caller should grab lock
    void sanity_check() const {
        // Check if full
        if (lru_map.size() == N) {
            // None of the counters should be 0
            for (const auto& c : counters) {
                assert(c > 0);
            }
        }

        // Check the converse
        auto min = next.load();
        for (const auto& c : counters) {
            if (c < min) min = c;
        }
        if (min > 0) {
            assert(lru_map.size() == N);
        }

        // Check that all ranges are coherent
        for (const auto& range : lru_map) {
            const auto low = range.first;
            const auto high = range.second.first;
            const auto idx = range.second.second;
            assert(low == low_keys[idx]);
            assert(high == high_keys[idx]);
        }
    }
    */

    bool weird_overlaps(K kl, K kh);

public:

    //typedef std::function<void(K, K)> purge_fn;

    // Construct a WS with at most `N` pages.
    WS();

    /*
    ~WS();

    // Update stats by registering a touch for key `k`. If a range should be
    // evicted, `f` is invoked on that range while holding an exclusive lock.
    //
    // Note that the _CALLER_ should verify that `k` is in a range that is
    // _already_ in the WS. If it is not, use the other `touch`.
    void touch(const K k, purge_fn f);
    void touch_no_lock(const K k, purge_fn f);

    // Grab the read lock ugh...
    void read_lock() {
        pthread_rwlock_rdlock(&lock);
    }
    int try_read_lock() {
        return pthread_rwlock_tryrdlock(&lock);
    }
    void read_unlock() {
        pthread_rwlock_unlock(&lock);
    }

    // Grab the write lock ugh...
    void write_lock() {
        pthread_rwlock_wrlock(&lock);
    }
    void write_unlock() {
        pthread_rwlock_unlock(&lock);
    }

    // Update stats by registering a touch for key `k` in range `[kl, kh)`
    // which should _not_ already be in the WS. If a range should be
    // evicted, `f` is invoked on that range while holding an exclusive lock.
    //
    // Returns true iff the new range became hot.
    //
    // Note that the _CALLER_ should verify that `[kl, kh)` is NOT _already_ in
    // the WS. If it is, use the other `touch`.
    bool touch(const K kl, const K kh, const K k, purge_fn f);
    bool touch_no_lock(const K kl, const K kh, const K k, purge_fn f);

    // Returns true iff the given key k is in a range in the WS.
    bool is_hot(const K& k);
    bool is_hot_no_lock(const K& k);
    */

    bool touch(const K& kl, const K& kh, const K& k);
    void remove(const K& kl, const K& kh);
    bool needs_purge() const;
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

    //int ret = pthread_rwlock_init(&lock, NULL);
    //assert(ret == 0);
}

//template <typename K, size_t N>
//WS<K, N>::~WS() {
//    int ret = pthread_rwlock_destroy(&lock);
//    assert(ret == 0);
//}

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
        set_mru(**maybe);
        return true;
    } else {
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

/*
template <typename K, size_t N>
void WS<K, N>::touch(const K k, purge_fn f) {
    pthread_rwlock_rdlock(&lock);

    touch_no_lock(k, f);

    pthread_rwlock_unlock(&lock);
}

template <typename K, size_t N>
void WS<K, N>::touch_no_lock(const K k, purge_fn) {

    //sanity_check();

    auto maybe = lru_map.find(k);
    assert(maybe);

    // Set to MRU
    set_mru(**maybe);

    //sanity_check();
}

template <typename K, size_t N>
bool WS<K, N>::touch(const K kl, const K kh, const K k, purge_fn f) {
    pthread_rwlock_wrlock(&lock);

    auto ret = touch_no_lock(kl, kh, k, f);

    pthread_rwlock_unlock(&lock);

    return ret;
}

template <typename K, size_t N>
bool WS<K, N>::touch_no_lock(const K kl, const K kh, const K k, purge_fn f) {
    assert(kl <= k && k < kh);

    //sanity_check();

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
        assert(lru_map.size() == N);
        counters[lru].store(0);
        evicted_low = low_keys[lru];
        evicted_high = high_keys[lru];
        // NOTE: we cannot assert that the range is [kl, kh) because it might have
        // changed since we inserted it.
        //
        // TODO: is there a possibility that we need to try both?
        lru_map.remove(evicted_low);

        assert(lru_map.size() == (N - 1));
    }

    //sanity_check();

    //auto already = lru_map.find(k);
    //if (already) {
    //    std::cout<<"duplicate key! " << kl << " " << kh << " " << k << " "
    //        << **already << " "
    //        << low_keys[**already] << " "
    //        << high_keys[**already] << " "
    //        << counters[**already] << " "
    //        << next << " "
    //        << std::endl;
    //    assert(false);
    //}

    // Insert new.
    low_keys[lru] = kl;
    high_keys[lru] = kh;
    lru_map.insert(kl, kh, lru);

    // Make MRU.
    set_mru(lru);

    //sanity_check();

    if (lru_counter > 0) {
        f(evicted_low, evicted_high);
    }

    //sanity_check();

    return true;
}

template <typename K, size_t N>
bool WS<K, N>::is_hot(const K& k) {
    pthread_rwlock_rdlock(&lock);
    auto maybe = lru_map.find(k);
    pthread_rwlock_unlock(&lock);

    return maybe;
}

template <typename K, size_t N>
bool WS<K, N>::is_hot_no_lock(const K& k) {
    auto maybe = lru_map.find(k);
    return maybe;
}
*/

} // namespace btree_hybrid

#endif
