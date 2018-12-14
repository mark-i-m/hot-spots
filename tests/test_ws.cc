
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
    assert(ws.touch(0, 10, 1));
    for (int i = 1; i < 2 * N; ++i) {
        assert(ws.touch(0, 10, 1));
    }

    // Evict LRU [0, 10)
    for (int i = 1; i < N; ++i) {
        assert(ws.touch(i * 10, i * 10 + 10, i * 10));
    }

    assert(!ws.touch(N * 10, N * 10 + 10, N * 10));

    assert(ws.needs_purge());
    Key kl, kh;
    std::tie(kl, kh) = ws.purge_range();
    assert(kl == 0 && kh == 10);
}

void test_simple_concurrent() {
    std::cout << "test_simple_concurrent" << std::endl;

    constexpr int TEST_SIZE = 1000000;
    constexpr int N_THREADS = 10;

    btree_hybrid::WS<Key, N_THREADS> ws;

    // Data and routine for all threads.
    auto f = [&ws](int tno) {
        ws.touch(tno * 10, tno * 10 + 10, tno * 10 + 4);

        for (int i = 0; i < TEST_SIZE; ++i) {
            ws.touch(tno * 10, tno * 10 + 10, tno * 10 + (i % 10));
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
