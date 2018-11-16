#ifndef _BTREE_HC_H_
#define _BTREE_HC_H_

#include "util.h"
#include <unordered_map>

namespace btree_hybrid {

template <typename K, typename V>
struct HC {
    // Creating a `Map` type from Keys to Values
    typedef std::unordered_map<K, V> Map;

    // Insert a key and value in a page that is already hot
    void insert(K k, V v);
    // Insert a range [kl, kh) into the RangeMap and subsequently insert the
    // key and value
    void insert(K kl, K kh, K k, V v);
    // Remove a range [kl, kh) from the RangeMap and return the corresponding
    // Keys and Values
    Map remove(const K& kl, const K& kh);
    // Given a Key k, return a value if it's present
    util::maybe::Maybe<V *> find(const K& k);

private:
    // Cached key and values for high contention pages
    util::RangeMap<K, Map> hot_cache;
};


template <typename K, typename V>
void HC<K, V>::insert(K k, V v) {
    auto maybe = hot_cache.find(k);
    assert(maybe);
    (*maybe)->insert(k, v);
}


template <typename K, typename V>
void HC<K, V>::insert(K kl, K kh, K k, V v) {
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
}

template <typename K, typename V>
typename HC<K, V>::Map HC<K, V>::remove(const K& kl, const K& kh) {
    return hot_cache.remove(kl, kh);
}


template <typename K, typename V>
util::maybe::Maybe<V *> HC<K, V>::find(const K& k) {
    auto maybe = hot_cache.find(k);
    if(!maybe) {
        return util::maybe::Maybe<V *>();
    } else {
        auto it = (*maybe)->find(k);
        if(it != (*maybe)->end()) {
            return util::maybe::Maybe<V *>(&it->second);
        } else {
            return util::maybe::Maybe<V *>();
        }
    }
}

} //namespace btree_hybrid

#endif
