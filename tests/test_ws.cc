
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
    ws.touch(0, 10, 1, [](Key, Key){
            assert(false);
            });
    for (int i = 1; i < 2 * N; ++i) {
        ws.touch(1, [](Key, Key) {
                assert(false);
                });
    }

    assert(ws.is_hot(5));

    // Evict LRU [0, 10)
    for (int i = 1; i < N; ++i) {
        ws.touch(i * 10, i * 10 + 10, i * 10, [](Key, Key){
                assert(false);
                });
    }

    for (int i = 1; i < N; ++i) {
        assert(ws.is_hot(i * 10 + 1));
    }

    assert(ws.is_hot(5));

    auto purged1 = false;
    ws.touch(N * 10, N * 10 + 10, N * 10, [&purged1](Key kl, Key kh){
            assert(kl == 0);
            assert(kh == 10);
            purged1 = true;
            });
    assert(purged1);
    assert(!ws.is_hot(5));

    // Touch the LRU
    ws.touch(15, [](Key, Key){
            assert(false);
            });

    auto purged2 = false;
    ws.touch(N * 100, N * 100 + 10, N * 100, [&purged2](Key kl, Key kh){
            assert(kl == 20);
            assert(kh == 30);
            purged2 = true;
            });
    assert(purged2);
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
        ws.touch(tno * 10, tno * 10 + 10, tno * 10 + 4, [](Key, Key){
                assert(false);
                });

        for (int i = 0; i < TEST_SIZE; ++i) {
            ws.touch(tno * 10 + (i % 10), [](Key, Key){
                    assert(false);
                    });
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
