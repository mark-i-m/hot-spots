#ifndef _BTREE_HYBRID_H_
#define _BTREE_HYBRID_H_

/*
 * This is a concurrent B-tree implementation that uses Optimisitic Lock
 * Coupling. It is taken from https://github.com/wangziqi2016/index-microbench.
 *
 * Lock-coupling is when you only lock at most two locks in the btree at a
 * time. If you use "optmisitic locks", you get Optimistic Lock Coupling (OLC).
 * The main goal of optimistic lock coupling is to minimize cache coherence
 * contention between cores, thus achieving higher performance and scalability.
 *
 * See the `OptLock` type for more on optimistic locking.
 */

#include "btree-base.h"
#include "ws.h"
#include "util.h"

#include <libcuckoo/cuckoohash_map.hh>
#include <algorithm>
#include <immintrin.h>
#include <sched.h>
#include <atomic>
#include <cassert>
#include <cstring>
#include <vector>
#include <pthread.h>

namespace btree_hybrid {
// Each page in the Btree can be either an inner node or a leaf node.
enum class PageType : uint8_t { BTreeInner = 1, BTreeLeaf = 2 };

// Use 4KB pages
static const uint64_t pageSize = 4 * 1024;

// An optimistic lock implementation.
//
// An optimistic lock has two parts: a lock and a version counter. The lock is
// acquired by all writers, as in a normal RW-locking scheme. However, a reader
// only waits for the lock to be free and gets the version number. On a
// WriteUnlock, we increment the version number. On a ReadUnlock, we check that
// the version number has not changed since we acquired the lock. If it has, we
// restart.
//
// Optimistic locking works best when conflicts are rare.
//
// This implementation comes more or less straight from the pseudo-code in
// appendix A of this paper: https://db.in.tum.de/~leis/papers/artsync.pdf.
struct OptLock {
    // In this implementation, both the lock and the version counter are
    // represented using this 64-bit word.
    //
    // Bit 0 represents that the locked value is obsolete (1 = obsolete).
    // Bit 1 represents that the value is locked (1 = locked).
    // Bits 2-63 represent the version counter.
    std::atomic<uint64_t> typeVersionLockObsolete{0b100};

    // Returns true if the given version represents a locked state.
    bool isLocked(uint64_t version) { return ((version & 0b10) == 0b10); }

    // Grab an optimistic read lock.
    //
    // If the current version is locked, set `needRestart` to true and return
    // the current version. The reader should restart if `needRestart` is set to
    // true after calling this method.
    uint64_t readLockOrRestart(bool &needRestart) {
        uint64_t version;
        version = typeVersionLockObsolete.load();
        if (isLocked(version) || isObsolete(version)) {
            // PAUSE compiler intrinsic. See
            // https://software.intel.com/en-us/node/524249.
            _mm_pause();
            needRestart = true;
        }
        return version;
    }

    // Grab the write lock.
    //
    // This is done by first grabbing the optimistic read lock, and then
    // attempting to upgrade it to a write lock. If this attempt fails,
    // `needRestart` is set to true, and the caller needs to restart.
    void writeLockOrRestart(bool &needRestart) {
        uint64_t version;
        version = readLockOrRestart(needRestart);
        if (needRestart) return;

        upgradeToWriteLockOrRestart(version, needRestart);
        if (needRestart) return;
    }

    // Upgrade the given read lock to a write lock.
    //
    // `version` should be the version at the time a read lock was acquired.
    //
    // If the version has changed since the read lock was acquired,
    // `needRestart` is set to true, and the caller needs to restart.
    void upgradeToWriteLockOrRestart(uint64_t &version, bool &needRestart) {
        if (typeVersionLockObsolete.compare_exchange_strong(version,
                                                            version + 0b10)) {
            version = version + 0b10;
        } else {
            _mm_pause();
            needRestart = true;
        }
    }

    // Release the write lock.
    //
    // This should only be called if you successfully acquired the write
    // lock. This method releases the lock and increments the version.
    void writeUnlock() { typeVersionLockObsolete.fetch_add(0b10); }

    // Return the obsolete bit of the given version.
    bool isObsolete(uint64_t version) { return (version & 1) == 1; }

    // The same as `readUnlockOrRestart`.
    void checkOrRestart(uint64_t startRead, bool &needRestart) const {
        readUnlockOrRestart(startRead, needRestart);
    }

    // Release the read lock.
    //
    // `startRead` is the version at the time the read lock was acquired.
    //
    // If the version has changed since the read lock was acquired,
    // `needRestart` is set to true, and the caller should restart.
    void readUnlockOrRestart(uint64_t startRead, bool &needRestart) const {
        needRestart = (startRead != typeVersionLockObsolete.load());
    }

    // Release the write lock _and_ set the obsolete bit.
    //
    // This is like `writeUnlock` except that it also sets the obsolete bit.
    void writeUnlockObsolete() { typeVersionLockObsolete.fetch_add(0b11); }
};

// A base type for all btree nodes. Each node hasi an optimisitc lock.
struct NodeBase : public OptLock {
    // Leaf or inner?
    PageType type;

    // The number of entries in this btree node.
    uint16_t count;
};

// Leaf superclass so that we don't have to keep defining the type.
struct BTreeLeafBase : public NodeBase {
    static const PageType typeMarker = PageType::BTreeLeaf;
};

// A single leaf node in the btree. Note that anyone doing operations on these
// nodes should already be holding a lock.
template <class Key, class Payload>
struct BTreeLeaf : public BTreeLeafBase {
    // Represents a key and value associated with that key.
    struct Entry {
        Key k;
        Payload p;
    };

    // The max number of entries in a leaf node (based on the size of keys
    // and pages). We also need to account for the space of the locks.
    static const uint64_t maxEntries =
        (pageSize - sizeof(NodeBase)) / (sizeof(Key) + sizeof(Payload));

    // The keys for each child.
    Key keys[maxEntries];

    // The (key, value) pairs inserted into the tree.
    Payload payloads[maxEntries];

    // Construct an empty leaf node.
    BTreeLeaf() {
        count = 0;
        type = typeMarker;

        // // debugging
        // for (size_t i = 0; i < maxEntries; ++i) {
        //     keys[i] = 0xDEADBEEF;
        //     payloads[i] = 0xDEADBEEF;
        // }
    }

    // Returns true if this leaf is full. It needs to be split before we can
    // take any more entries.
    bool isFull() { return count == maxEntries; };

    // Returns the index into this node of the least key that is greater than
    // or equal to `k`.
    unsigned lowerBound(Key k) {
        unsigned lower = 0;
        unsigned upper = count;
        do {
            unsigned mid = ((upper - lower) / 2) + lower;
            if (k < keys[mid]) {
                upper = mid;
            } else if (k > keys[mid]) {
                lower = mid + 1;
            } else {
                return mid;
            }
        } while (lower < upper);
        return lower;
    }

    // Alternate implementation of `lowerBound`. This function is not used anywhere.
    unsigned lowerBoundBF(Key k) {
        auto base = keys;
        unsigned n = count;
        while (n > 1) {
            const unsigned half = n / 2;
            base = (base[half] < k) ? (base + half) : base;
            n -= half;
        }
        return (*base < k) + base - keys;
    }

    // Insert the new (key, value) pair into this leaf. The caller should make
    // sure that the node has space and split it if necessary.
    void insert(Key k, Payload p) {
        assert(count < maxEntries);
        if (count) {
            unsigned pos = lowerBound(k);
            if ((pos < count) && (keys[pos] == k)) {
                // Upsert
                payloads[pos] = p;
                return;
            }
            memmove(keys + pos + 1, keys + pos, sizeof(Key) * (count - pos));
            memmove(payloads + pos + 1, payloads + pos,
                    sizeof(Payload) * (count - pos));
            keys[pos] = k;
            payloads[pos] = p;
        } else {
            keys[0] = k;
            payloads[0] = p;
        }
        count++;
    }

    // Split this leaf node in half, and return the new leaf node. The new node
    // comes _after_ this node.
    BTreeLeaf *split(Key &sep) {
        BTreeLeaf *newLeaf = new BTreeLeaf();
        newLeaf->count = count - (count / 2);
        count = count - newLeaf->count;
        memcpy(newLeaf->keys, keys + count, sizeof(Key) * newLeaf->count);
        memcpy(newLeaf->payloads, payloads + count,
               sizeof(Payload) * newLeaf->count);
        sep = keys[count - 1];

        // // debugging
        // for (size_t i = count; i < maxEntries; ++i) {
        //     keys[i] = 0xDEADBEEF;
        //     payloads[i] = 0xDEADBEEF;
        // }

        return newLeaf;
    }
};

// Inner node superclass so that we don't have to keep defining the type.
struct BTreeInnerBase : public NodeBase {
    static const PageType typeMarker = PageType::BTreeInner;
};

// A single inner node in the btree. Note that anyone doing operations on these
// nodes should already be holding a lock.
template <class Key>
struct BTreeInner : public BTreeInnerBase {
    // The max number of entries in an inner node (based on the size of keys
    // and pages). We also need to account for the space of the locks.
    static const uint64_t maxEntries =
        (pageSize - sizeof(NodeBase)) / (sizeof(Key) + sizeof(NodeBase *));

    // Pointers to the child nodes.
    NodeBase *children[maxEntries];

    // The keys for each child.
    Key keys[maxEntries];

    // Construct an empty inner node.
    BTreeInner() {
        count = 0;
        type = typeMarker;

        // // debugging
        // for (size_t i = 0; i < maxEntries; ++i) {
        //     keys[i] = 0xDEADBEEF;
        // }
    }

    // Returns true if adding one more key would fill the node.
    bool isFull() { return count == (maxEntries - 1); };

    // Returns the index into this node of the least key that is greater than
    // or equal to `k`.
    unsigned lowerBound(Key k) {
        unsigned lower = 0;
        unsigned upper = count;
        do {
            unsigned mid = ((upper - lower) / 2) + lower;
            if (k < keys[mid]) {
                upper = mid;
            } else if (k > keys[mid]) {
                lower = mid + 1;
            } else {
                return mid;
            }
        } while (lower < upper);
        return lower;
    }

    // Alternate implementation of `lowerBound`. This function is not used anywhere.
    unsigned lowerBoundBF(Key k) {
        auto base = keys;
        unsigned n = count;
        while (n > 1) {
            const unsigned half = n / 2;
            base = (base[half] < k) ? (base + half) : base;
            n -= half;
        }
        return (*base < k) + base - keys;
    }

    // Split this inner node in half, and return the new inner node. The new
    // node comes _after_ this node.
    BTreeInner *split(Key &sep) {
        BTreeInner *newInner = new BTreeInner();
        newInner->count = count - (count / 2);
        count = count - newInner->count - 1;
        sep = keys[count];
        memcpy(newInner->keys, keys + count + 1,
               sizeof(Key) * (newInner->count + 1));
        memcpy(newInner->children, children + count + 1,
               sizeof(NodeBase *) * (newInner->count + 1));

        // // debugging
        // for (size_t i = count; i < maxEntries; ++i) {
        //     keys[i] = 0xDEADBEEF;
        // }

        return newInner;
    }

    // Insert the new child with the given key into this inner node. The caller
    // should make sure that the node has space and split it if necessary.
    void insert(Key k, NodeBase *child) {
        assert(count < maxEntries - 1);
        unsigned pos = lowerBound(k);
        memmove(keys + pos + 1, keys + pos, sizeof(Key) * (count - pos + 1));
        memmove(children + pos + 1, children + pos,
                sizeof(NodeBase *) * (count - pos + 1));
        keys[pos] = k;
        children[pos] = child;
        std::swap(children[pos], children[pos + 1]);
        count++;
    }
};

// A generic, thread-safe btree using OLC and our cache. It is a modification
// of the OLC implementation from the CMU Bw-tree critique paper.
template <class Key, class Value, size_t WSSize = 10>

struct BTree : public common::BTreeBase<Key, Value> {
    // The root node of the btree.
    std::atomic<NodeBase *> root;

    // WS is the policy layer of the Hybrid B-tree. It originally stood for
    // "Working Set", but it is really an LRU approximation.
    WS<Key, WSSize> ws;

    // The caching layer itself. This is just a concurrent hashmap.
    cuckoohash_map<Key, Value> hc;

    // This lock is grabbed to do "purges" (evictions from the cache of a range
    // of values). It is only ever grabbed by insertions; lookups don't need to
    // grab any locks.
    //
    // It is only grabbed as a write lock when a thread does a purge.
    // Otherwise, it is grabbed as a read lock.
    mutable pthread_rwlock_t big_lock;

    // Construct a new btree with exactly one node, which is an empty leaf node.
    BTree() {
        root = new BTreeLeaf<Key, Value>();

        int ret = pthread_rwlock_init(&big_lock, NULL);
        assert(ret == 0);
    }

    ~BTree() {
        int ret = pthread_rwlock_destroy(&big_lock);
        assert(ret == 0);
    }

    // Create a new root node with the two given nodes as children separated by
    // the given key. Atomically replace the current root with the new one.
    void makeRoot(Key k, NodeBase *leftChild, NodeBase *rightChild) {
        auto inner = new BTreeInner<Key>();
        inner->count = 1;
        inner->keys[0] = k;
        inner->children[0] = leftChild;
        inner->children[1] = rightChild;
        root = inner;
    }

    // Depending on the value of `count`, either yield the processor to the OS
    // scheduler or inform the processor you are waiting for a spin lock.
    void yield(int count) {
        if (count > 3)
            sched_yield();
        else
            _mm_pause();
    }

    // A helper to traverse the B-tree, grabbing appropriate locks. It is used by
    // the `bulk_insert` routine, which is used for purges.
    //
    // This routine returns the page that key `k` would be on. It grabs the
    // page's write lock before returning it. The caller is responsible for
    // unlocking. Moreover, the routine also returns the maximum value that can
    // be on this page without updating the parent's keys; this max value is
    // `Nothing` if this is the rightmost leaf.
    //
    // In the process, zero or more B-tree nodes may be split.
    //
    // The `no_split` flag is used for debugging. A panic occurs if this flag
    // is true and a node is split by this routine.
    std::pair<BTreeLeaf<Key, Value>*, util::maybe::Maybe<Key>>
    bulk_insert_traverse(Key k, bool no_split = false) {
         int restartCount = 0;
    restart:
        if (restartCount++) yield(restartCount);
        bool needRestart = false;

        // Current node
        NodeBase *node = root;
        uint64_t versionNode = node->readLockOrRestart(needRestart);
        if (needRestart || (node != root)) goto restart;

        // Parent of current node
        BTreeInner<Key> *parent = nullptr;
        uint64_t versionParent = 0;
        uint16_t parent_idx;
        bool is_rightmost = true;

        while (node->type == PageType::BTreeInner) {
            auto inner = static_cast<BTreeInner<Key> *>(node);

            // Split eagerly if full
            if (inner->isFull()) {
                assert(!no_split);

                // Lock
                if (parent) {
                    parent->upgradeToWriteLockOrRestart(versionParent,
                                                        needRestart);
                    if (needRestart) goto restart;
                }
                node->upgradeToWriteLockOrRestart(versionNode, needRestart);
                if (needRestart) {
                    if (parent) parent->writeUnlock();
                    goto restart;
                }
                if (!parent && (node != root)) {  // there's a new parent
                    node->writeUnlock();
                    goto restart;
                }
                // Split
                Key sep;
                BTreeInner<Key> *newInner = inner->split(sep);
                if (parent)
                    parent->insert(sep, newInner);
                else
                    makeRoot(sep, inner, newInner);
                // Unlock and restart
                node->writeUnlock();
                if (parent) parent->writeUnlock();
                goto restart;
            }

            if (parent) {
                parent->readUnlockOrRestart(versionParent, needRestart);
                if (needRestart) goto restart;
            }

            parent = inner;
            versionParent = versionNode;
            parent_idx = inner->lowerBound(k);

            // descend to the left
            if (parent_idx < inner->count - 1) {
                is_rightmost = false;
            }

            node = inner->children[parent_idx];
            inner->checkOrRestart(versionNode, needRestart);
            if (needRestart) goto restart;
            versionNode = node->readLockOrRestart(needRestart);
            if (needRestart) goto restart;
        }

        auto leaf = static_cast<BTreeLeaf<Key, Value> *>(node);

        // Split leaf if full
        if (leaf->count == leaf->maxEntries) {
            assert(!no_split);

            // Lock
            if (parent) {
                parent->upgradeToWriteLockOrRestart(versionParent, needRestart);
                if (needRestart) goto restart;
            }
            node->upgradeToWriteLockOrRestart(versionNode, needRestart);
            if (needRestart) {
                if (parent) parent->writeUnlock();
                goto restart;
            }
            if (!parent && (node != root)) {  // there's a new parent
                node->writeUnlock();
                goto restart;
            }
            // Split
            Key sep;
            BTreeLeaf<Key, Value> *newLeaf = leaf->split(sep);
            if (parent)
                parent->insert(sep, newLeaf);
            else
                makeRoot(sep, leaf, newLeaf);
            // Unlock and restart
            node->writeUnlock();
            if (parent) parent->writeUnlock();
            goto restart;
        } else {
            // only lock leaf node
            node->upgradeToWriteLockOrRestart(versionNode, needRestart);
            if (needRestart) goto restart;
            util::maybe::Maybe<Key> leaf_max; // Nothing by default
            if (parent) {
                if(!is_rightmost) {
                    // Set max value before potentially returning
                    leaf_max = util::maybe::Maybe<Key>(parent->keys[parent_idx]);
                }
                parent->readUnlockOrRestart(versionParent, needRestart);
                if (needRestart) {
                    node->writeUnlock();
                    goto restart;
                }
            }
            return std::pair<BTreeLeaf<Key, Value>*, util::maybe::Maybe<Key>>{leaf, leaf_max};  // success
        }
    }

    // Does an optimized insertion of the list of key-value pairs into the
    // B-tree. This routine is _only_ to be called if the caller is holding the
    // `big_lock`. It is only used for purges.
    void bulk_insert(std::vector<std::pair<Key, Value>> key_values) {
        if (key_values.empty()) {
            return;
        }

        // sorted list of key/value pairs
        std::sort(key_values.begin(), key_values.end());

        // insertions
        auto it = key_values.begin();
        while(it != key_values.end()) {
            // Find leaf of insertion... locked
            BTreeLeaf<Key, Value>* l;
            util::maybe::Maybe<Key> leaf_max;
            std::tie(l, leaf_max) = bulk_insert_traverse(it->first);

            auto new_elements = 0;
            auto end = it;

            // Find the elements we can insert now while respecting the amount
            // of free space and the max key.
            while (it != key_values.end()
                    && (l->maxEntries - l->count) - new_elements > 0
                    && !(leaf_max && it->first >= *leaf_max)) {
                ++it; ++end;
                ++new_elements;
            }

            --end;

            // it == first element we haven't looked at yet
            // end == last element to insert in this iteration
            // new_elements == number elements to insert

            // Merge the existing entries with the purged entries in sorted
            // order from the end.  Doing it from the end means that we always
            // have space and only need to move each element once.
            //
            // "end" idx == l->count + new_elements - 1
            auto keys_end_idx = l->count + new_elements - 1; // ultimately will be the end idx
            auto to_insert = new_elements; // number elements remaining to insert from purged
            int existing_end_idx = l->count - 1; // current last inserted idx

            while (to_insert > 0) {
                if (existing_end_idx < 0 || end->first > l->keys[existing_end_idx]) { // insert *end
                    l->keys[keys_end_idx] = std::move(end->first);
                    l->payloads[keys_end_idx] = std::move(end->second);
                    --end;
                    --to_insert;
                } else {
                    l->keys[keys_end_idx] = std::move(l->keys[existing_end_idx]);
                    l->payloads[keys_end_idx] = std::move(l->payloads[existing_end_idx]);
                    --existing_end_idx;
                }

                --keys_end_idx;
            }

            // Update stats
            l->count += new_elements;

            // unlock leaf
            l->writeUnlock();

            // need to do a normal insertion to ensure space. This uses the
            // normal node splitting stuff to simplify the design.
            if(it != key_values.end()) {
                insert_inner(it->first, it->second, true);
                ++it;
            }
        }
    }

    // Grab the `big_lock` as a reader.
    void big_read_lock() const {
        pthread_rwlock_rdlock(&big_lock);
    }

    // Grab the `big_lock` as a writer.
    void big_write_lock() const {
        pthread_rwlock_wrlock(&big_lock);
    }

    // Release the `big_lock` as a reader or writer.
    void big_unlock() const {
        pthread_rwlock_unlock(&big_lock);
    }

    // The insert routine of the B-tree. This is a thread-safe insertion of the
    // (k, v) pair into this B-tree.
    //
    // It is a thin wrapper around `insert_inner` which does the heavy lifting.
    void insert(Key k, Value v) {
        insert_inner(k, v, false);
    }

    // Insert the (k, v) pair into the tree thread-safely. If `in_bulk_insert`
    // is true, avoid all paths that may interact with the policy or cache
    // layers. In other words, if `in_bulk_insert`, this routine behaves just
    // like the normal B-tree with OLC.
    //
    // Everything in this routine is normal until we have found a B-tree leaf.
    // See the comment at that point. For simplicity, we always traverse the
    // tree before doing anything with the cache.
    void insert_inner(Key k, Value v, bool in_bulk_insert) {
        int restartCount = 0;
    restart:
        if (restartCount++) yield(restartCount);
        bool needRestart = false;

        // Current node
        NodeBase *node = root;
        uint64_t versionNode = node->readLockOrRestart(needRestart);
        if (needRestart || (node != root)) goto restart;

        // Parent of current node
        BTreeInner<Key> *parent = nullptr;
        uint64_t versionParent = 0;

        // Keep track of some properties as we descend the tree
        bool is_root = true;
        bool is_leftmost = true;
        bool is_rightmost = true;

        Key min_parent_key = 0,
            max_parent_key = 0; // only valid if not root or leftmost

        while (node->type == PageType::BTreeInner) {
            auto inner = static_cast<BTreeInner<Key> *>(node);

            // The leaf cannot be the root
            is_root = false;

            // Split eagerly if full
            if (inner->isFull()) {
                // Lock
                if (parent) {
                    parent->upgradeToWriteLockOrRestart(versionParent,
                                                        needRestart);
                    if (needRestart) goto restart;
                }
                node->upgradeToWriteLockOrRestart(versionNode, needRestart);
                if (needRestart) {
                    if (parent) parent->writeUnlock();
                    goto restart;
                }
                if (!parent && (node != root)) {  // there's a new parent
                    node->writeUnlock();
                    goto restart;
                }
                // Split
                Key sep;
                BTreeInner<Key> *newInner = inner->split(sep);
                if (parent)
                    parent->insert(sep, newInner);
                else
                    makeRoot(sep, inner, newInner);
                // Unlock and restart
                node->writeUnlock();
                if (parent) parent->writeUnlock();
                goto restart;
            }

            if (parent) {
                parent->readUnlockOrRestart(versionParent, needRestart);
                if (needRestart) goto restart;
            }

            parent = inner;
            versionParent = versionNode;

            const uint16_t parent_idx = inner->lowerBound(k);

            // descend to the left
            if (parent_idx < inner->count - 1) {
                is_rightmost = false;
            }
            // descend to the right
            else if (parent_idx > 0) {
                is_leftmost = false;
            }

            // If we are inserting either in the rightmost or leftmost node,
            // make up a max. It doesn't matter too much, but there may be a
            // pathological case.
            if (is_rightmost) {
                min_parent_key = inner->keys[parent_idx];
                max_parent_key = min_parent_key + inner->maxEntries;
            } else if (is_leftmost) {
                max_parent_key = inner->keys[parent_idx];
                min_parent_key = max_parent_key - inner->maxEntries;
            } else {
                min_parent_key = inner->keys[parent_idx];
                max_parent_key = inner->keys[parent_idx + 1];
            }

            node = inner->children[parent_idx];
            inner->checkOrRestart(versionNode, needRestart);
            if (needRestart) goto restart;
            versionNode = node->readLockOrRestart(needRestart);
            if (needRestart) goto restart;
        }

        auto leaf = static_cast<BTreeLeaf<Key, Value> *>(node);

        // Split leaf if full
        if (leaf->count == leaf->maxEntries) {
            // Lock
            if (parent) {
                parent->upgradeToWriteLockOrRestart(versionParent, needRestart);
                if (needRestart) goto restart;
            }
            node->upgradeToWriteLockOrRestart(versionNode, needRestart);
            if (needRestart) {
                if (parent) parent->writeUnlock();
                goto restart;
            }
            if (!parent && (node != root)) {  // there's a new parent
                node->writeUnlock();
                goto restart;
            }
            // Split
            Key sep;
            BTreeLeaf<Key, Value> *newLeaf = leaf->split(sep);
            if (parent)
                parent->insert(sep, newLeaf);
            else
                makeRoot(sep, leaf, newLeaf);
            // Unlock and restart
            node->writeUnlock();
            if (parent) parent->writeUnlock();
            goto restart;
        } else {
            // This is the code path in which we actually might use the
            // cache/policy. Here are some invariants:
            // - The policy is always aware of what is in the cache. There is
            //   nothing in the cache that the policy did not approve.
            // - Once data is in the tree, it must always be in either the
            //   cache or the tree (or possibly both).
            //
            // In this code, we simplify things by using the `big_lock` to
            // manage concurrency for both the cache and policy layers.
            //
            // For the most part, there is only ever one lock held at any given
            // time.  In some places, we do some lock crabbing, though. This
            // makes it _way_ easier to avoid deadlocks.

            // Handling the root node is a bit weird so... just don't use the
            // cache for the root.
            if (!is_root && !in_bulk_insert) {
                // If we are inserting a new minimum or maximum key in the
                // B-tree, make up the other endpoint of the range we are
                // inserting to the cache.
                if (k < min_parent_key) {
                    min_parent_key = k - leaf->maxEntries;
                    max_parent_key = k + 1;
                } else if (k >= max_parent_key) {
                    min_parent_key = k;
                    max_parent_key = k + leaf->maxEntries;
                }

                // Grab the big read lock and check if a purge is needed.
                // Holding the read lock prevents a purge from happening while
                // we are checking.
                big_read_lock();
                if (ws.needs_purge()) {
                    big_unlock();

                    // If we found that purge is needed, then we grab the big
                    // write lock to attempt to do the purge.
                    //
                    // We need to make sure that a purge is still needed since
                    // another thread could have done it between the previous
                    // line and the next one.
                    //
                    // After a purge, we restart. This simplifies the logic
                    // somewhat and means that the common case has no write
                    // locks on it.
                    //
                    // We also purge even if we didn't restart because it makes
                    // life easier.

                    big_write_lock();
                    if (ws.needs_purge()) {
                        // Purge the range of values indicated by the policy.
                        // To do this, we briefly lock the hash table to search
                        // for keys in the range. This is really just an
                        // unfortunate consequence of the fact that the
                        // concurrent hash map we choose doesn't have an
                        // efficient way to iterate over all buckets.
                        //
                        // FIXME: unforunately, this is extremely sloooooowww...
                        Key purge_low, purge_high;
                        std::tie(purge_low, purge_high) = ws.purge_range();
                        std::vector<std::pair<Key, Value>> key_values;
                        auto locked_hc = hc.lock_table();
                        for (auto it : locked_hc) {
                            if (it.first >= purge_low && it.first < purge_high) {
                                key_values.push_back(it);
                            }
                        }
                        // We can unlock here because lookup threads don't
                        // mutate the cache, and we are the only insertion
                        // thread with the big writer lock.
                        locked_hc.unlock();

                        // Do a bulk insertion here. This must happen before
                        // the keys are removed from the cache so that readers
                        // don't miss keys.
                        bulk_insert(key_values);

                        // Remove from cache and inform policy that the range
                        // is gone.
                        ws.remove(purge_low, purge_high);
                        for (auto it : key_values) {
                            hc.erase(it.first);
                        }
                    }
                    big_unlock();
                    goto restart;
                } else {
                    // At this point, we are still holding the big read lock.

                    // If no purging is needed, we can just inform the policy
                    // layer of the touch (to update its stats). `touch`
                    // returns true if the key is in a hot range. The policy
                    // (`ws`) is thread-safe, so multiple threads holding the
                    // big read lock can safely do this.
                    if (ws.touch(min_parent_key, max_parent_key, k)) { // if is hot
                        // cache insert. The hash map handles concurrent updates.
                        hc.insert(k, v);
                        big_unlock();
                        return;
                    } else {
                        // release big read lock. This avoids deadlocks since we
                        // may need to restart. Holding the read lock any longer
                        // wouldn't be useful anyway.
                        big_unlock();

                        // do a normal B-tree insertion.

                        // Release the parent's "read lock" (possibly restart) after
                        // grabbing the node's write lock.
                        node->upgradeToWriteLockOrRestart(versionNode, needRestart);
                        if (needRestart) goto restart;
                        if (parent) {
                            parent->readUnlockOrRestart(versionParent, needRestart);
                            if (needRestart) {
                                node->writeUnlock();
                                goto restart;
                            }
                        }

                        // Normal B-tree insertion.
                        leaf->insert(k, v);
                        node->writeUnlock();
                        return;
                    }
                }
            } else {
                // Normal B-tree insertion.

                // only lock leaf node
                node->upgradeToWriteLockOrRestart(versionNode, needRestart);
                if (needRestart) goto restart;
                if (parent) {
                    parent->readUnlockOrRestart(versionParent, needRestart);
                    if (needRestart) {
                        node->writeUnlock();
                        goto restart;
                    }
                }

                leaf->insert(k, v);
                node->writeUnlock();
                return;
            }
        }
    }

    // Lookup key `k` in the btree. If `k` is in the btree, set `result` to the
    // value associated with `k` and return true. If `k` is not in the btree,
    // return false.
    bool lookup(Key k, Value &result) {
        // No locks needed. Concurrency is handled by the hash map :P
        if (hc.find(k, result)) {
            return true;
        }

        // If cache miss, then do normal B-tree lookup. Insertions maintain the
        // invariant that if a key is inserted, then it is always in at least
        // one of the cache or the tree. So if we get here, we had a cache
        // miss, and if we still can't find the key in the tree, then it is not
        // in the structure at all.

        int restartCount = 0;
    restart:
        if (restartCount++) yield(restartCount);
        bool needRestart = false;

        NodeBase *node = root;
        uint64_t versionNode = node->readLockOrRestart(needRestart);
        if (needRestart || (node != root)) goto restart;

        // Parent of current node
        BTreeInner<Key> *parent = nullptr;
        uint64_t versionParent = 0;

        while (node->type == PageType::BTreeInner) {
            auto inner = static_cast<BTreeInner<Key> *>(node);

            if (parent) {
                parent->readUnlockOrRestart(versionParent, needRestart);
                if (needRestart) goto restart;
            }

            parent = inner;
            versionParent = versionNode;

            node = inner->children[inner->lowerBound(k)];
            inner->checkOrRestart(versionNode, needRestart);
            if (needRestart) goto restart;
            versionNode = node->readLockOrRestart(needRestart);
            if (needRestart) goto restart;
        }

        BTreeLeaf<Key, Value> *leaf =
            static_cast<BTreeLeaf<Key, Value> *>(node);

        unsigned pos = leaf->lowerBound(k);
        bool success;
        if ((pos < leaf->count) && (leaf->keys[pos] == k)) {
            success = true;
            result = leaf->payloads[pos];
        }
        if (parent) {
            parent->readUnlockOrRestart(versionParent, needRestart);
            if (needRestart) goto restart;
        }
        node->readUnlockOrRestart(versionNode, needRestart);
        if (needRestart) goto restart;

        return success;
    }

    // NOTE: This does not work. It's just a copy of the implementation from
    // the OLC. This implementation does not actually account for the cache.
    //
    // Do a range query on the btree. Starting with the least key greater than
    // or equal to `k`, scan at most `range` values into the buffer pointed to
    // by `output`. Return the number of elements read. Note that we may read
    // fewer than `range` elements even if there are more elements that we
    // could scan.  The caller should keep calling `scan` until no records are
    // read.
    uint64_t scan(Key k, int range, Value *output) {
        int restartCount = 0;
    restart:
        if (restartCount++) yield(restartCount);
        bool needRestart = false;

        NodeBase *node = root;
        uint64_t versionNode = node->readLockOrRestart(needRestart);
        if (needRestart || (node != root)) goto restart;

        // Parent of current node
        BTreeInner<Key> *parent = nullptr;
        uint64_t versionParent = 0;

        while (node->type == PageType::BTreeInner) {
            auto inner = static_cast<BTreeInner<Key> *>(node);

            if (parent) {
                parent->readUnlockOrRestart(versionParent, needRestart);
                if (needRestart) goto restart;
            }

            parent = inner;
            versionParent = versionNode;

            node = inner->children[inner->lowerBound(k)];
            inner->checkOrRestart(versionNode, needRestart);
            if (needRestart) goto restart;
            versionNode = node->readLockOrRestart(needRestart);
            if (needRestart) goto restart;
        }

        BTreeLeaf<Key, Value> *leaf =
            static_cast<BTreeLeaf<Key, Value> *>(node);
        unsigned pos = leaf->lowerBound(k);
        int count = 0;
        for (unsigned i = pos; i < leaf->count; i++) {
            if (count == range) break;
            output[count++] = leaf->payloads[i];
        }

        if (parent) {
            parent->readUnlockOrRestart(versionParent, needRestart);
            if (needRestart) goto restart;
        }
        node->readUnlockOrRestart(versionNode, needRestart);
        if (needRestart) goto restart;

        return count;
    }
};

}  // namespace btree_hybrid

#endif
