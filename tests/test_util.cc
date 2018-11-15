
#include "util.h"

#include <iostream>

void test_maybe();

int main() {
    test_maybe();

    std::cout << "SUCCESS :)" << std::endl;
}

void test_maybe() {
    std::cout << "test_maybe" << std::endl;

    uint64_t x = 1234;

    util::Maybe<uint64_t> m1;
    util::Maybe<uint64_t> m2(3456);
    util::Maybe<uint64_t&> m3(x);

    assert(!m1.is_value());

    assert(m2.is_value());
    assert(m2.get_value() == 3456);

    assert(m3.is_value());
    assert(m3.get_value() == x);
    m3.get_value() = 0xAFAFAFA;
    assert(x == 0xAFAFAFA);
}
