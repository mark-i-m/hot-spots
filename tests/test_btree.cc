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
#include <thread>

// Btree implementations to test
enum class BTreeType {
    BTreeOLC = 1,
    BTreeHybrid = 2,
    BTreeByteReorder = 3,
};

// All tests use the same type, for simplicity.
using Key = int64_t;
using Value = int64_t;

// Test prototypes
void test_simple_insert_read(common::BTreeBase<Key, Value> *btree);
void test_insert_read(common::BTreeBase<Key, Value> *btree);
void test_insert_read_over(common::BTreeBase<Key, Value> *btree);
void test_insert_read_concurrent_seq(common::BTreeBase<Key, Value> *btree);
void test_insert_read_concurrent_rand(common::BTreeBase<Key, Value> *btree);
void test_insert_read_concurrent_contend(common::BTreeBase<Key, Value> *btree);

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
            default:
                // should never happen
                assert(false);
                return nullptr;
        }
    };

    // Run tests
    test_simple_insert_read(new_btree_fn());
    test_insert_read(new_btree_fn());
    test_insert_read_over(new_btree_fn());
    test_insert_read_concurrent_contend(new_btree_fn());
    test_insert_read_concurrent_seq(new_btree_fn());
    test_insert_read_concurrent_rand(new_btree_fn());

    // Done!
    std::cout << "SUCCESS :)" << std::endl;
}

// Insert a pair and read it back.
void test_simple_insert_read(common::BTreeBase<Key, Value> *btree) {
    std::cout << "test_simple_insert_read" << std::endl;

    btree->insert(0, 0);

    Value v;
    assert(btree->lookup(0, v));
    assert(v == 0);
}

// Insert a large number of random k,v pairs, and read them back.
void test_insert_read(common::BTreeBase<Key, Value> *btree) {
    std::cout << "test_insert_read" << std::endl;

    constexpr int TEST_SIZE = 100000;

    //const auto pairs = gen_data<Key, Value>(TEST_SIZE);
    const auto pairs = gen_data<Key, Value>(TEST_SIZE);
    for (const auto& pair : pairs) {
        btree->insert(pair);
    }

    for (const auto& pair : pairs) {
        Value v;
        bool found = btree->lookup(pair.first, v);
        assert(found);
        assert(v == pair.second);
    }
}

// Insert a number of random k,v pairs, and read them back. Repeat.
void test_insert_read_over(common::BTreeBase<Key, Value> *btree) {
    std::cout << "test_insert_read_over" << std::endl;

    constexpr int TEST_SIZE = 10000;
    constexpr int TEST_REPEAT = 100;

    //const auto pairs = gen_data<Key, Value>(TEST_SIZE);
    const auto pairs = gen_data<Key, Value>(TEST_SIZE);
    for (int i = 0; i < TEST_REPEAT; ++i) {
        for (const auto& pair : pairs) {
            btree->insert(pair);
        }

        for (const auto& pair : pairs) {
            Value v;
            bool found = btree->lookup(pair.first, v);
            assert(found);
            assert(v == pair.second);
        }
    }
}

// Insert a large number of sequential k,v pairs concurrently, and read them back.
void test_insert_read_concurrent_seq(common::BTreeBase<Key, Value> *btree) {
    std::cout << "test_insert_read_concurrent_seq" << std::endl;

    constexpr int TEST_SIZE = 1000000;
    constexpr int N_THREADS = 10;

    // Data and routine for all threads.
    const auto pairs = gen_data_seq<Key, Value>(TEST_SIZE);
    auto f = [&pairs, btree]() {
        for (const auto& pair : pairs) {
            btree->insert(pair);
        }

        for (const auto& pair : pairs) {
            Value v;
            bool found = btree->lookup(pair.first, v);
            assert(found);
            assert(v == pair.second);
        }
    };

    // Start threads.
    std::vector<std::thread> threads;
    for (int i = 0; i < N_THREADS; ++i) {
        threads.push_back(std::thread(f));
    }

    // Wait for threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
}

// Insert a large number of random k,v pairs concurrently, and read them back.
void test_insert_read_concurrent_rand(common::BTreeBase<Key, Value> *btree) {
    std::cout << "test_insert_read_concurrent_rand" << std::endl;

    constexpr int TEST_SIZE = 1000000;
    constexpr int N_THREADS = 10;

    // Data and routine for all threads.
    const auto pairs = gen_data<Key, Value>(TEST_SIZE);
    auto f = [&pairs, btree]() {
        for (const auto& pair : pairs) {
            btree->insert(pair);
        }

        for (const auto& pair : pairs) {
            Value v;
            bool found = btree->lookup(pair.first, v);
            assert(found);
            assert(v == pair.second);
        }
    };

    // Start threads.
    std::vector<std::thread> threads;
    for (int i = 0; i < N_THREADS; ++i) {
        threads.push_back(std::thread(f));
    }

    // Wait for threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
}

// Insert the same key over and over again
void test_insert_read_concurrent_contend(common::BTreeBase<Key, Value> *btree) {
    std::cout << "test_insert_read_concurrent_contend" << std::endl;

    constexpr int TEST_SIZE = 1000000;
    constexpr int N_THREADS = 10;

    const Key k = 0xDEADBEEF;
    const Value v = 0xCAFEBABE;

    // Data and routine for all threads.
    auto f = [btree]() {
        for (int i = 0; i < TEST_SIZE; ++i) {
            Key key = k + (i % 4000);
            btree->insert(key, v);

            Value foundv;
            bool found = btree->lookup(key, foundv);
            assert(found);
            assert(v == foundv);
        }
    };

    // Start threads.
    std::vector<std::thread> threads;
    for (int i = 0; i < N_THREADS; ++i) {
        threads.push_back(std::thread(f));
    }

    // Wait for threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
}
