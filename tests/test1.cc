/*
 * A test harness to check correctness of btree implementations.
 */

#include "btree-base.h"
#include "btreeolc.h"
#include "btree-hybrid.h"
#include "btree-bytereorder.h"

#include <cassert>

// Btree implementations to test
enum class BTreeType {
    BTreeOLC = 1,
    BTreeHybrid = 2,
    BTreeByteReorder = 3,
};

// Test prototypes
void test1(common::BTreeBase<uint64_t, uint64_t> *btree);

int main() {
    // TODO: Based on command line args, choose which tree to construct.
    BTreeType type = BTreeType::BTreeOLC;

    // Construct the btree implementation we want to test.
    common::BTreeBase<uint64_t, uint64_t> *btree;
    switch (type) {
        case BTreeType::BTreeOLC:
            btree = new btreeolc::BTree<uint64_t, uint64_t>();
            break;
        case BTreeType::BTreeHybrid:
            btree = new btree_hybrid::BTree<uint64_t, uint64_t>();
            break;
        case BTreeType::BTreeByteReorder:
            btree = new btree_bytereorder::BTree<uint64_t, uint64_t>();
            break;
    }

    // Run tests
    test1(btree);
}

void test1(common::BTreeBase<uint64_t, uint64_t> *) {
    assert(false);
    // TODO
}
