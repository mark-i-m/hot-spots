#ifndef _BTREE_BTREE_BASE_H
#define _BTREE_BTREE_BASE_H

#include <stdint.h>

namespace common {

/*
 * A base class for thread-safe btree maps.
 *
 * All implementations should satisfy this interface. This helps with testing
 * and benchmarking.
 */
template <class Key, class Value>
struct BTreeBase {
    // Insert the (k, v) pair into the tree.
    virtual void insert(Key k, Value v) = 0;

    // Lookup key `k` in the btree. If `k` is in the btree, set `result` to the
    // value associated with `k` and return true. If `k` is not in the btree,
    // return false.
    virtual bool lookup(Key k, Value &result) = 0;

    // Do a range query on the btree. Starting with the least key greater than
    // or equal to `k`, scan at most `range` values into the buffer pointed to
    // by `output`. Return the number of elements read. Note that we may read
    // fewer than `range` elements even if there are more elements that we
    // could scan.  The caller should keep calling `scan` until no records are
    // read.
    virtual uint64_t scan(Key k, int range, Value *output) = 0;

    // For convenience: insert from pair.
    template <class Pair>
    void insert(Pair p) {
        insert(p.first, p.second);
    }
};

} // namespace common

#endif
