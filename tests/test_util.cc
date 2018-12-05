
#include "util.h"

#include <iostream>

void test_maybe();
void test_range_map_simple();

int main() {
    test_maybe();
    test_range_map_simple();

    std::cout << "SUCCESS :)" << std::endl;
}

void test_maybe() {
    std::cout << "test_maybe" << std::endl;

    uint64_t x = 1234;

    util::maybe::Maybe<uint64_t> m1;
    util::maybe::Maybe<uint64_t> m2(3456);
    util::maybe::Maybe<uint64_t*> m3(&x);

    assert(!m1);

    assert(m2);
    assert(*m2 == 3456);

    assert(m3);
    assert(**m3 == x);
    **m3 = 0xAFAFAFA;
    assert(x == 0xAFAFAFA);
}

void test_range_map_simple() {
    std::cout << "test_range_map_simple" << std::endl;

    util::RangeMap<uint64_t, uint64_t> rm;

    assert(!rm.find(0xDEADBEEF));
    assert(rm.size() == 0);

    rm.insert(0, 10, 20);

    assert(!rm.find(0xDEADBEEF));
    assert(rm.find(0));
    assert(**rm.find(0) == 20);
    assert(rm.size() == 1);

    rm.insert(30, 40, 5);

    assert(!rm.find(0xDEADBEEF));
    assert(rm.find(0));
    assert(**rm.find(0) == 20);
    assert(rm.find(34));
    assert(**rm.find(34) == 5);
    assert(rm.size() == 2);

    // Remove
    uint64_t v = rm.remove(0);
    assert(v == 20);
    assert(!rm.find(20));
    assert(rm.size() == 1);

    uint64_t v2 = rm.remove(30);
    assert(v2 == 5);
    assert(!rm.find(20));
    assert(!rm.find(30));
    assert(rm.size() == 0);
}
