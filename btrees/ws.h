#ifndef _BTREE_WS_H_
#define _BTREE_WS_H_

// TODO : Implement the LRU alternately by keeping an atomic counter. On a
// touch, this value will be stored with the range in the RangeMap

#include "util.h"

#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <pthread.h>

namespace btree_hybrid {

// Tracks stats for ranges of pages that have been touched and chooses ranges
// to evict.
template <typename K>
class WS {
    struct RangeNode {
        K kl;
        K kh;

        RangeNode *next;
        RangeNode *prev;

        // Constructs a new RangeNode with the given range.
        RangeNode(K kl, K kh) : kl(kl), kh(kh), next(nullptr), prev(nullptr) {}

        // Debug
        void print() {
            RangeNode *node = this;
            do {
                std::cout << " [" << node->kl << ", " << node->kh << ")";
                node = node->next;
            } while (node != this);
            std::cout << std::endl;
        }
    };

    size_t n;
    util::RangeMap<K, RangeNode *> stats;
    RangeNode *lru_list;

    // Set the given node to the MRU.
    void set_mru(RangeNode * mru);

    // Remove this node from the LRU list.
    void remove_from_list(RangeNode * node);

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
    util::maybe::Maybe<std::pair<K, K>> touch(const K kl, const K kh, const K k);

    // Returns true iff the given key k is in a range in the WS.
    bool is_hot(const K& k);

    // Clears all stats.
    void clear();
};

////////////////////////////////////////////////////////////////////////////////
// Implementations
////////////////////////////////////////////////////////////////////////////////

template <typename K>
void WS<K>::set_mru(RangeNode * mru) {
    assert(mru);

    // First element
    if (!lru_list) {
        lru_list = mru;
        mru->next = mru;
        mru->prev = mru;
    } else {
        mru->next = lru_list;
        mru->prev = lru_list->prev;

        lru_list->prev = mru;
        mru->prev->next = mru;

        lru_list = mru;
    }
}

template <typename K>
void WS<K>::remove_from_list(RangeNode * node) {
    // If this is the only node
    if (node->next == node) {
        assert(node->prev == node);
        lru_list = nullptr;
    } else {
        // If this is the head of the lru_list
        if (lru_list == node) {
            lru_list = node->next;
        }

        // Update prev and next nodes
        node->next->prev = node->prev;
        node->prev->next = node->next;

        node->next = nullptr;
        node->prev = nullptr;
    }
}

template <typename K>
WS<K>::WS(size_t n) : n(n), lru_list(nullptr) {}

template <typename K>
util::maybe::Maybe<std::pair<K, K>> WS<K>::touch(const K k) {
    auto maybe = stats.find(k);
    assert(maybe);

    // Set to MRU
    remove_from_list(**maybe);
    set_mru(**maybe);

    return util::maybe::Maybe<std::pair<K, K>>();
}

template <typename K>
util::maybe::Maybe<std::pair<K, K>> WS<K>::touch(const K kl, const K kh, const K k) {
    assert(kl <= k && k < kh);

    auto maybe = stats.find(k);

    // If the key is not even in the map, insert it.
    if (!maybe) {
        RangeNode *r = new RangeNode(kl, kh);

        // Set to MRU
        set_mru(r);
        stats.insert(kl, kh, r);

        // Do we need to evict something?
        if (stats.size() > n) {
            assert(lru_list);

            auto lru = lru_list->prev;

            remove_from_list(lru);
            auto kl_lru = lru->kl;
            auto kh_lru = lru->kh;
            stats.remove(kl_lru, kh_lru);

            delete lru;

            return util::maybe::Maybe<std::pair<K, K>>({kl_lru, kh_lru});
        }
    }

    // Otherwise, update the stats.
    else {
        // Set to MRU
        remove_from_list(**maybe);
        set_mru(**maybe);
    }

    // Maybe to evict
    return util::maybe::Maybe<std::pair<K, K>>();
}

template <typename K>
bool WS<K>::is_hot(const K& k) {
    return stats.find(k);
}

template <typename K>
void WS<K>::clear() {
    stats.clear();
}

} // namespace btree_hybrid

#endif
