
#include "util.h"
#include "ws.h"

#include <iostream>

// All tests use the same type, for simplicity.
using Key = uint64_t;

void test_simple();

int main() {
    test_simple();

    std::cout << "SUCCESS :)" << std::endl;
}

void test_simple() {
    std::cout << "test_simple" << std::endl;

    constexpr int N = 10;

    btree_hybrid::WS<Key, N> ws;

    // No evictions
    assert(!ws.touch(0, 10, 1));
    for (int i = 1; i < 2 * N; ++i) {
        assert(!ws.touch(1));
    }

    assert(ws.is_hot(5));

    // Evict LRU [0, 10)
    for (int i = 1; i < N; ++i) {
        assert(!ws.touch(i * 10, i * 10 + 10, i * 10));
    }

    for (int i = 1; i < N; ++i) {
        assert(ws.is_hot(i * 10 + 1));
    }

    assert(ws.is_hot(5));

    auto maybe = ws.touch(N * 10, N * 10 + 10, N * 10);
    assert(maybe);
    assert(maybe->first == 0);
    assert(maybe->second == 10);
    assert(!ws.is_hot(5));

    // Touch the LRU
    assert(!ws.touch(15));

    auto maybe2 = ws.touch(N * 100, N * 100 + 10, N * 100);
    assert(maybe2);
    assert(maybe2->first == 20);
    assert(maybe2->second == 30);
    assert(!ws.is_hot(5));
    assert(ws.is_hot(15));
    assert(!ws.is_hot(25));
}
