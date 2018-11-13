/*
 * A test harness to check correctness of btree implementations.
 */

#include "btree-base.h"
#include "btreeolc.h"
#include "btree-hybrid.h"
#include "btree-bytereorder.h"

#include <cassert>
#include <iostream>
#include <string.h>

// Btree implementations to test
enum class BTreeType {
    BTreeOLC = 1,
    BTreeHybrid = 2,
    BTreeByteReorder = 3,
};

// Test prototypes
void test_simple_insert_read(common::BTreeBase<uint64_t, uint64_t> *btree);

void usage_and_exit() {
    std::cout << "Usage: ./test <TREE TYPE>\n"
        << "<TREE TYPE> := olc|hybrid|br"
        << std::endl;
    exit(1);
}

int main(int argc, char **argv) {
    // Make sure at least one cmd line arg
    if (argc < 2) {
        usage_and_exit();
    }

    // TODO: Based on command line args, choose which tree to construct.
    BTreeType type;
    if (strncmp("olc", argv[1], 4) == 0) {
        type = BTreeType::BTreeOLC;
    } else if (strncmp("hybrid", argv[1], 7) == 0) {
        type = BTreeType::BTreeHybrid;
    } else if (strncmp("br", argv[1], 3) == 0) {
        type = BTreeType::BTreeByteReorder;
    } else {
        usage_and_exit();
    }

    // Construct the btree implementation we want to test.
    common::BTreeBase<uint64_t, uint64_t> *btree;
    switch (type) {
        case BTreeType::BTreeOLC:
            std::cout << "Testing OLC" << std::endl;
            btree = new btreeolc::BTree<uint64_t, uint64_t>();
            break;
        case BTreeType::BTreeHybrid:
            std::cout << "Testing Hybrid" << std::endl;
            btree = new btree_hybrid::BTree<uint64_t, uint64_t>();
            break;
        case BTreeType::BTreeByteReorder:
            std::cout << "Testing Byte Reordering" << std::endl;
            btree = new btree_bytereorder::BTree<uint64_t, uint64_t>();
            break;
    }

    // Run tests
    test_simple_insert_read(btree);

    // Done!
    std::cout << "SUCCESS :)" << std::endl;
}

void test_simple_insert_read(common::BTreeBase<uint64_t, uint64_t> *btree) {
    btree->insert(0, 0);

    uint64_t v;
    assert(btree->lookup(0, v));
    assert(v == 0);
}
