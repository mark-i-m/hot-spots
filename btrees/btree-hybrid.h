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
#include "hc.h"
#include "util.h"

#include <immintrin.h>
#include <sched.h>
#include <atomic>
#include <cassert>
#include <cstring>

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

// A generic, thread-safe btree using OLC.
template <class Key, class Value, size_t WSSize = 10>
struct BTree : public common::BTreeBase<Key, Value> {
    // The root node of the btree.
    std::atomic<NodeBase *> root;

    WS<Key, WSSize> ws;
    HC<Key, Value> hc;

    // Construct a new btree with exactly one node, which is an empty leaf node.
    BTree() { root = new BTreeLeaf<Key, Value>(); }

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

    // Insert the (k, v) pair into the tree.
    void insert(Key k, Value v) {
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

        while (node->type == PageType::BTreeInner) {
            auto inner = static_cast<BTreeInner<Key> *>(node);

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

            node = inner->children[inner->lowerBound(k)];
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
            return;  // success
        }
    }

    // Lookup key `k` in the btree. If `k` is in the btree, set `result` to the
    // value associated with `k` and return true. If `k` is not in the btree,
    // return false.
    bool lookup(Key k, Value &result) {
        auto hc_find = hc.find(k);
        if (hc_find) {
            result = *hc_find;
            return true;
        }

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
