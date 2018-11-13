/*
 * A test harness to check correctness of btree implementations.
 */

#include "test-utils.h"

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

// All tests use the same type, for simplicity.
using Key = uint64_t;
using Value = uint64_t;

// Test prototypes
void test_simple_insert_read(common::BTreeBase<Key, Value> *btree);
void test_insert_read(common::BTreeBase<Key, Value> *btree);

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

    // Parse command line arg
    BTreeType type = BTreeType::BTreeOLC;
    if (strncmp("olc", argv[1], 4) == 0) {
        std::cout << "Testing OLC" << std::endl;
        type = BTreeType::BTreeOLC;
    } else if (strncmp("hybrid", argv[1], 7) == 0) {
        std::cout << "Testing Hybrid" << std::endl;
        type = BTreeType::BTreeHybrid;
    } else if (strncmp("br", argv[1], 3) == 0) {
        std::cout << "Testing Byte Reordering" << std::endl;
        type = BTreeType::BTreeByteReorder;
    } else {
        usage_and_exit();
    }

    // Construct the btree implementation we want to test.
    auto new_btree_fn = [type]() -> common::BTreeBase<Key, Value> * {
        switch (type) {
            case BTreeType::BTreeOLC:
                return new btreeolc::BTree<Key, Value>();
            case BTreeType::BTreeHybrid:
                return new btree_hybrid::BTree<Key, Value>();
            case BTreeType::BTreeByteReorder:
                return new btree_bytereorder::BTree<Key, Value>();
        }
    };

    // Run tests
    test_simple_insert_read(new_btree_fn());
    test_insert_read(new_btree_fn());

    // Done!
    std::cout << "SUCCESS :)" << std::endl;
}

void test_simple_insert_read(common::BTreeBase<Key, Value> *btree) {
    btree->insert(0, 0);

    Value v;
    assert(btree->lookup(0, v));
    assert(v == 0);
}

void test_insert_read(common::BTreeBase<Key, Value> *btree) {
    constexpr int TEST_SIZE = 1000000;

    const auto pairs = gen_data<Key, Value>(TEST_SIZE);
    for (const auto pair : pairs) {
        btree->insert(pair);
    }

    for (const auto pair : pairs) {
        Value v;
        bool found = btree->lookup(pair.first, v);
        assert(found);
        assert(v == pair.second);
    }
}
