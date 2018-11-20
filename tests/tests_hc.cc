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
    assert(*hc.find(2) == 6);
    auto map = hc.remove(0, 10);
    assert(!hc.find(2));
    assert(map.size() == 1);
    assert(map.find(2) != map.end());
    assert(map.find(2)->second == 6);
    hc.insert(0, 10, 2, 6);
    hc.insert(0, 10, 1, 3);
    hc.insert(0, 10, 3, 9);
    hc.insert(0, 10, 5, 15);
    hc.insert(0, 10, 4, 12);
    auto map_find = hc.get_all(0, 10);
    for(const auto& pair : map_find) {
        assert(pair.second == 3 * pair.first);
    }
    assert(map_find.size() == 5);
}
