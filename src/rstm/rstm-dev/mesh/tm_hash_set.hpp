/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  tm_hash_set.hpp
 *
 *  Simple hash-based set.
 *  Element type must be the same size as unsigned long.
 *  Currently has only built-in hash function, which is designed to work
 *  well for pointers.
 */

#ifndef TM_HASH_SET_HPP__
#define TM_HASH_SET_HPP__

#include "tm_set.hpp"
#include "tm_list_set.hpp"

template<typename T>
class tm_hash_set : public tm_set<T>
{
    tm_list_set<T> **bucket;
    int num_buckets;

    tm_hash_set(const tm_hash_set&);
    // no implementation; forbids passing by value
    tm_hash_set& operator=(const tm_hash_set&);
    // no implementation; forbids copying

    // Hash function that should work reasonably well for pointers.
    // Basically amounts to cache line number.
    //
    TRANSACTION_SAFE unsigned long hash(T item)
    {
        // verbose attributing to avoid gcc error
        union {
            T from;
            unsigned long to;
        } cast = { item };
        return (cast.to >> 6) % num_buckets;
    }

  public:
    TRANSACTION_SAFE virtual void insert(const T item)
    {
        int b = hash(item);
        bucket[b]->insert(item);
    }

    TRANSACTION_SAFE virtual void remove(const T item)
    {
        int b = hash(item);
        bucket[b]->remove(item);
    }

    virtual bool lookup(const T item)
    {
        return bucket[hash(item)]->lookup(item);
    }

    virtual void apply_to_all(void (*f)(T item))
    {
        for (int i = 0; i < num_buckets; i++)
            bucket[i]->apply_to_all(f);
    }

    tm_hash_set(int capacity)
    {
        // get space for buckets (load factor 1.5)
        num_buckets = capacity + (capacity >> 1);

        bucket = new tm_list_set<T>*[num_buckets];
        for (int i = 0; i < num_buckets; i++)
            bucket[i] = new tm_list_set<T>();
    }

    // Destruction not currently supported.
    virtual ~tm_hash_set() { assert(false); }

    // for debugging (non-thread-safe):
    void print_stats()
    {
        for (int b = 0; b < num_buckets; b++) {
            if (b % 8 == 0)
                cout << "\n";
            cout << "\t" << bucket[b]->size();
        }
        if (num_buckets % 8 != 0)
            cout << "\n";
    }
};

#endif // TM_HASH_SET_HPP__
