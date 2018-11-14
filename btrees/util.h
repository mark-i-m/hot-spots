#pragma once

#include <map>
#include <unordered_map>

namespace util {

// Is either a `T` or nothing.
template <typename T>
class Maybe {
    bool _is_val;
    T* v;

public:
    // Construct a `Maybe` that contains nothing.
    Maybe() : _is_val(false) {}

    // Construct a `Maybe` that contains value `v`.
    Maybe(T& v) : _is_val(true), v(&v) {}

    // Returns true iff this `Maybe` contains a value.
    bool is_value() const {
        return _is_val;
    }

    // Returns the value in this `Maybe`. You should call `is_value` first to
    // check that there is a value. It is undefined behavior to call this
    // method if there is no value.
    T& get_value() const {
        return *v;
    }
};

// A map from ranges of type `K` to values of type `T`.
template <typename K, typename T>
class RangeMap {
private:
    // Contains the actual data. Has the following structure:
    //   low key -> (high key, k-v mappings)
    std::map<K, std::pair<K, std::unordered_map<K, T>>> ranges;

    // Given a key, find the range it is mapped to.
    Maybe<std::unordered_map<K, T>&> find_range(const K& k);

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
    Maybe<T&> lookup(const K& k);

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
Maybe<std::unordered_map<K, T>&> RangeMap<K, T>::find_range(const K& k) {
    auto it = ranges.upper_bound(k);
    if (it == ranges.begin()) {
        return Maybe<std::unordered_map<K, T>&>();
    } else {
        --it;
    }

    if (it.second.first > k) {
        return Maybe<std::unordered_map<K, T>&>(it.second.second);
    } else {
        return Maybe<std::unordered_map<K, T>&>();
    }
}

template <typename K, typename T>
RangeMap<K, T>::RangeMap() {
}

template <typename K, typename T>
void RangeMap<K, T>::insert_range(K kl, K kh) {
    ranges.insert(kl, {kh, std::unordered_map<K, T>()});
}

template <typename K, typename T>
void RangeMap<K, T>::insert_key(K k, T v) {
    auto& range = find_range();
    assert(range.is_val());
    range.get_value().insert(k, v);
}

template <typename K, typename T>
Maybe<T&> RangeMap<K, T>::lookup(const K& k) {
    auto& range = find_range();
    if (!range.is_val()) {
        return Maybe<T&>();
    }
    auto& val = range.get_value().find(k);
    if (val == range.get_value().end()) {
        return Maybe<T&>();
    } else {
        return Maybe<T&>(*val);
    }
}

template <typename K, typename T>
void RangeMap<K, T>::clear() {
    ranges.clear();
}

} // namespace util
