#ifndef _TESTS_TEST_UTILS_H_
#define _TESTS_TEST_UTILS_H_

#include <cstdlib>
#include <vector>
#include <set>

/*
 * Some helpful utilities for writing tests.
 */

// Generate `n` (k,v) pairs where all keys are unique.
template <typename Key, typename Value>
std::vector<std::pair<Key, Value>> gen_data(const size_t n) {
    std::vector<std::pair<Key, Value>> pairs;
    std::set<Key> keys;

    // Generate a bunch of keys and values.
    srand(0);

    for (size_t i = 0; i < n; ++i) {
        Key k = rand();
        Value v = rand();

        // Unique keys
        if (keys.count(k) > 0) {
            continue;
        }

        keys.insert(k);
        pairs.push_back({k, v});
    }

    return pairs;
}

template <typename Key, typename Value>
std::vector<std::pair<Key, Value>> gen_data_seq(const size_t n) {
    std::vector<std::pair<Key, Value>> pairs;

    for (size_t i = 0; i < n; ++i) {
        pairs.push_back({i, i});
    }

    return pairs;
}

#endif
