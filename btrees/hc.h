#ifndef _BTREE_HC_H_
#define _BTREE_HC_H_

// TODO: finer-grained locking

#include "util.h"
#include <unordered_map>
#include <pthread.h>

namespace btree_hybrid {

template <typename K, typename V>
struct HC {
    // Creating a `Map` type from Keys to Values
    typedef std::unordered_map<K, V> Map;
    // Lock
    mutable pthread_rwlock_t lock;

    HC();

    ~HC();

    // Insert a key and value in a page that is already hot
    void insert(K k, V v);
    // Insert a range [kl, kh) into the RangeMap and subsequently insert the
    // key and value
    void insert(K kl, K kh, K k, V v);
    // Given a range, get all the (Key, Value) pairs belonging to that range
    // It's assumed that the range [kl, kh) is a distinct range already present
    // in the hot_cache
    Map get_all(K kl, K kh);
    // Remove a range [kl, kh) from the RangeMap and return the corresponding
    // Keys and Values
    Map remove(const K& kl, const K& kh);
    // Given a Key k, return a value if it's present
    util::maybe::Maybe<V> find(const K& k);

private:
    // Cached key and values for high contention pages
    util::RangeMap<K, Map> hot_cache;
};

template <typename K, typename V>
HC<K, V>::HC() {
    int ret = pthread_rwlock_init(&lock, NULL);
    assert(ret == 0);
}

template <typename K, typename V>
HC<K, V>::~HC() {
    int ret = pthread_rwlock_destroy(&lock);
    assert(ret == 0);
}

template <typename K, typename V>
void HC<K, V>::insert(K k, V v) {
    pthread_rwlock_wrlock(&lock);
    auto maybe = hot_cache.find(k);
    assert(maybe);
    (*maybe)->insert({k, v});
    pthread_rwlock_unlock(&lock);
}


template <typename K, typename V>
void HC<K, V>::insert(K kl, K kh, K k, V v) {
    pthread_rwlock_wrlock(&lock);
    auto maybe = hot_cache.find(k);

    // If the range is not in the RangeMap, insert it along with the given Key and Value
    if (!maybe) {
        Map m;
        m.insert({k, v});
        hot_cache.insert(kl, kh, m);
    }
    // Otherwise, insert directly
    else {
        (*maybe)->insert({k, v});
    }
    pthread_rwlock_unlock(&lock);
}

template <typename K, typename V>
typename HC<K, V>::Map HC<K, V>::get_all(K kl, K) {
    // TODO : We're returning a pointer, lookout for SEGFAULTS!!
    auto maybe = hot_cache.find(kl);
    if (maybe) {
        return **maybe;
    } else {
        return typename HC<K, V>::Map();
    }
}

template <typename K, typename V>
typename HC<K, V>::Map HC<K, V>::remove(const K& kl, const K& kh) {
    return hot_cache.remove(kl, kh);
}


template <typename K, typename V>
util::maybe::Maybe<V> HC<K, V>::find(const K& k) {
    pthread_rwlock_rdlock(&lock);
    auto maybe = hot_cache.find(k);
    if(!maybe) {
        pthread_rwlock_unlock(&lock);
        return util::maybe::Maybe<V>();
    } else {
        auto it = (*maybe)->find(k);
        if(it != (*maybe)->end()) {
            // We used to return a pointer here, but the pointer could be
            // invalidated later if the element is evicted, so instead, we just
            // return a copy.
            auto maybe = util::maybe::Maybe<V>(it->second);
            pthread_rwlock_unlock(&lock);
            return maybe;
        } else {
            pthread_rwlock_unlock(&lock);
            return util::maybe::Maybe<V>();
        }
    }
}

} //namespace btree_hybrid

#endif
