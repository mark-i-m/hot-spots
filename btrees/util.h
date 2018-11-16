#ifndef _BTREE_UTIL_H_
#define _BTREE_UTIL_H_

#include <map>
#include <unordered_map>
#include <cassert>
#include <type_traits>
#include <pthread.h>

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
    //   low key -> (high key, T)
    std::map<K, std::pair<K, T>> ranges;

    mutable pthread_rwlock_t lock;

public:
    // Create an empty range map.
    RangeMap();

    // Destroy a range map
    ~RangeMap();

    // Map range [kl, kh) to value v. For simplicity and performance, we
    // assume that the _CALLER_ checks that no two ranges overlap and checks
    // that kl < kh.
    void insert(K kl, K kh, T v);

    // Given a key, find the range it is mapped to.
    maybe::Maybe<T *> find(const K& k);

    // Remove range [kl, kh) from the `RangeMap`. For simplicity and
    // performance, we assume that the _CALLER_ checks that [kl, kh) is in the
    // map.
    T remove(const K& kl, const K& kh);

    // Returns the number of ranges in the map.
    size_t size() const {
        return ranges.size();
    }
};

////////////////////////////////////////////////////////////////////////////////
// Implementations
////////////////////////////////////////////////////////////////////////////////

template <typename K, typename T>
maybe::Maybe<T *> RangeMap<K, T>::find(const K& k) {
    pthread_rwlock_rdlock(&lock);
    auto it = ranges.upper_bound(k);
    if (it == ranges.begin()) {
        pthread_rwlock_unlock(&lock);
        return maybe::Maybe<T *>();
    } else {
        --it;
    }

    // it->second.first is the high key of the range.
    if (it->second.first > k) {
        auto maybe = maybe::Maybe<T *>(&it->second.second);
        pthread_rwlock_unlock(&lock);
        return maybe;
    } else {
        pthread_rwlock_unlock(&lock);
        return maybe::Maybe<T *>();
    }
}

template <typename K, typename T>
RangeMap<K, T>::RangeMap() {
    int ret = pthread_rwlock_init(&lock, NULL);
    assert(ret == 0);
}

template <typename K, typename T>
RangeMap<K, T>::~RangeMap() {
    int ret = pthread_rwlock_destroy(&lock);
    assert(ret == 0);
}

template <typename K, typename T>
void RangeMap<K, T>::insert(K kl, K kh, T v) {
    pthread_rwlock_wrlock(&lock);
    ranges.insert({kl, {kh, v}});
    pthread_rwlock_unlock(&lock);
}

template <typename K, typename T>
T RangeMap<K, T>::remove(const K& kl, const K& kh) {
    pthread_rwlock_wrlock(&lock);
    auto it = ranges.find(kl);
    assert(it->second.first == kh);

    T v = it->second.second;
    ranges.erase(it);
    pthread_rwlock_unlock(&lock);

    return v;
}

} // namespace util

#endif
