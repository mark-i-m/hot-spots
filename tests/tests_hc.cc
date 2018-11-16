#include "util.h"
#include "hc.h"
#include <iostream>

// All tests use the same type, for simplicity.
using Key = uint64_t;
using Value = uint64_t;

void test_simple();

int main() {
    test_simple();

    std::cout << "SUCCESS :)" << std::endl;
}

void test_simple() {
    btree_hybrid::HC<Key, Value> hc;
    assert(!hc.find(2));
    hc.insert(0, 10, 2, 6);
    assert(hc.find(2));
    assert(**hc.find(2) == 6);
    **hc.find(2) += 1;
    auto map = hc.remove(0, 10);
    assert(!hc.find(2));
    assert(map.size() == 1);
    assert(map.find(2) != map.end());
    assert(map.find(2)->second == 7);
}
