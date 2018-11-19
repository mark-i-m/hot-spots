
#include "util.h"
#include "ws.h"

#include <iostream>
#include <vector>
#include <thread>

// All tests use the same type, for simplicity.
using Key = uint64_t;

void test_simple();
void test_simple_concurrent();

int main() {
    test_simple();
    test_simple_concurrent();

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

void test_simple_concurrent() {
    std::cout << "test_simple_concurrent" << std::endl;

    constexpr int TEST_SIZE = 1000000;
    constexpr int N_THREADS = 10;

    btree_hybrid::WS<Key, N_THREADS> ws;

    // Data and routine for all threads.
    auto f = [&ws](int tno) {
        assert(!ws.touch(tno * 10, tno * 10 + 10, tno * 10 + 4));

        for (int i = 0; i < TEST_SIZE; ++i) {
            assert(!ws.touch(tno * 10 + (i % 10)));
        }
    };

    // Start threads.
    std::vector<std::thread> threads;
    for (int i = 0; i < N_THREADS; ++i) {
        threads.push_back(std::thread(f, i));
    }

    // Wait for threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
}
