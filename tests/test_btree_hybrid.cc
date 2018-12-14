#include "test-utils.h"

#include "btree-hybrid.h"

#include <unistd.h>
#include <cassert>
#include <iostream>
#include <string.h>
#include <thread>

using Key = int64_t;
using Value = int64_t;

void test_btree_hybrid_bulk_insert();
void test_btree_hybrid_bulk_insert_gap();
void test_btree_hybrid_bulk_insert_rand();

int main() {
    test_btree_hybrid_bulk_insert();
    test_btree_hybrid_bulk_insert_gap();
    test_btree_hybrid_bulk_insert_rand();
    return 0;
}

void test_btree_hybrid_bulk_insert() {
    std::cout << "test_btree_hybrid_bulk_insert" << std::endl;

    constexpr size_t N = 1000000;
    btree_hybrid::BTree<Key, Value> btree;

    // Init
    {
        auto init_pairs = gen_data_seq<Key, Value>(1000);
        for (const auto pair : init_pairs) {
            btree.insert_inner(pair.first, pair.second, true);
        }
    }

    auto key_values = gen_data_seq<Key, Value>(N);
    btree.bulk_insert(key_values);
    for(const auto pair : key_values) {
        Value v;
        bool found = btree.lookup(pair.first, v);
        assert(found);
        //assert(v == pair.second);
        if (v != pair.second) {
            std::cout << "expected " << pair.second << ", got " << v << std::endl;
        }
    }
}

void test_btree_hybrid_bulk_insert_gap() {
    std::cout << "test_btree_hybrid_bulk_insert_gap" << std::endl;

    constexpr size_t N = 1000000;
    btree_hybrid::BTree<Key, Value> btree;

    // Init
    {
        auto init_pairs = gen_data_seq<Key, Value>(1000);
        for (const auto pair : init_pairs) {
            btree.insert_inner(pair.first, pair.second, true);
        }
        btree.insert_inner(10000000, 0xDEADBEEF, true);
    }

    auto key_values = gen_data_seq<Key, Value>(N);
    btree.bulk_insert(key_values);
    for(const auto pair : key_values) {
        Value v;
        bool found = btree.lookup(pair.first, v);
        assert(found);
        //assert(v == pair.second);
        if (v != pair.second) {
            std::cout << "expected " << pair.second << ", got " << v << std::endl;
        }
    }
}

void test_btree_hybrid_bulk_insert_rand() {
    std::cout << "test_btree_hybrid_bulk_insert_rand" << std::endl;

    constexpr size_t N = 1000;
    btree_hybrid::BTree<Key, Value> btree;

    auto key_values_all = gen_data<Key, Value>(N);
    auto half = key_values_all.size() / 2;
    std::vector<std::pair<Key, Value>> key_values_init(
            key_values_all.begin(), key_values_all.begin() + half);
    std::vector<std::pair<Key, Value>> key_values(
            key_values_all.begin() + half, key_values_all.end());

    // Init
    for (const auto pair : key_values_init) {
        btree.insert_inner(pair.first, pair.second, true);
    }

    btree.bulk_insert(key_values);
    for(const auto pair : key_values) {
        Value v;
        bool found = btree.lookup(pair.first, v);
        if (!found) {
            std::cout << "missing " << pair.first << std::endl;
        }
        assert(found);
        //assert(v == pair.second);
        if (v != pair.second) {
            std::cout << "expected " << pair.second << ", got " << v << std::endl;
        }
        //std::cout<<"ok"<<std::endl;
    }
}

