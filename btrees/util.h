#ifndef _BTREE_UTIL_H_
#define _BTREE_UTIL_H_

#include <map>
#include <unordered_map>
#include <cassert>
#include <type_traits>

namespace util {

namespace maybe {

// A type that either contains a value or nothing.
//
// Example:
//
// if (my_maybe) {
//   do_something(*my_maybe);
// }
//
// NOTE: T must not be a reference type.
template <typename T>
class Maybe
{
    typename std::aligned_storage<sizeof(T), alignof(T)>::type v;
    bool is_value;

public:
    // Construct a `Maybe` with nothing in it.
    Maybe() : is_value(false) {}

    // Construct a `Maybe` with `v` in it.
    Maybe(T val) : is_value(true) {
        new (&v) T(std::move(val));
    }

    // Destructor
    ~Maybe() {
        if (is_value) {
            reinterpret_cast<T *>(&v)->~T();
        }
    }

    // True iff there is a value in this `Maybe`.
    operator bool() const {
        return is_value;
    }

    // Returns a reference to the value in this `Maybe`. The caller should
    // verify that there is a value.
    T& operator *() {
        assert(is_value);
        return *reinterpret_cast<T *>(&v);
    }

    T* operator ->() {
        assert(is_value);
        return reinterpret_cast<T *>(&v);

    }
};

} // namespace maybe

// A map from ranges of type `K` to values of type `T`.
template <typename K, typename T>
class RangeMap {
private:
    // Contains the actual data. Has the following structure:
    //   low key -> (high key, k-v mappings)
    std::map<K, std::pair<K, std::unordered_map<K, T>>> ranges;

    // Given a key, find the range it is mapped to.
    maybe::Maybe<std::unordered_map<K, T> *> find_range(const K& k);

public:
    // Create an empty range map.
    RangeMap();

    // Insert range [kl, kh) into the map. For simplicity and performance, we
    // assume that the _CALLER_ checks that no two ranges overlap and checks
    // that kl < kh.
    void insert_range(K kl, K kh);

    // Insert (k, v) into the map. The _CALLER_ must ensure that there exists a
    // range containing `k`.
    void insert_key(K k, T v);

    // Lookup a key k in the map and return the associated value if it exists.
    maybe::Maybe<T *> lookup(const K& k);

    // Clear all entries in the map.
    void clear();

    // Returns the number of ranges in the map.
    size_t size() const {
        return ranges.size();
    }
};

////////////////////////////////////////////////////////////////////////////////
// Implementations
////////////////////////////////////////////////////////////////////////////////

template <typename K, typename T>
maybe::Maybe<std::unordered_map<K, T> *> RangeMap<K, T>::find_range(const K& k) {
    auto it = ranges.upper_bound(k);
    if (it == ranges.begin()) {
        return maybe::Maybe<std::unordered_map<K, T> *>();
    } else {
        --it;
    }

    if (it->second.first > k) {
        return maybe::Maybe<std::unordered_map<K, T> *>(&it->second.second);
    } else {
        return maybe::Maybe<std::unordered_map<K, T> *>();
    }
}

template <typename K, typename T>
RangeMap<K, T>::RangeMap() {
}

template <typename K, typename T>
void RangeMap<K, T>::insert_range(K kl, K kh) {
    ranges.insert({kl, {kh, std::unordered_map<K, T>()}});
}

template <typename K, typename T>
void RangeMap<K, T>::insert_key(K k, T v) {
    auto range = find_range(k);
    assert(range);
    (*range)->insert({k, v});
}

template <typename K, typename T>
maybe::Maybe<T *> RangeMap<K, T>::lookup(const K& k) {
    using Maybe = maybe::Maybe<T *>;

    auto range = find_range(k);
    if (!range) {
        return Maybe();
    }
    auto val = (*range)->find(k);
    if (val == (*range)->end()) {
        return Maybe();
    } else {
        return Maybe(&val->second);
    }
}

template <typename K, typename T>
void RangeMap<K, T>::clear() {
    ranges.clear();
}

} // namespace util

#endif
