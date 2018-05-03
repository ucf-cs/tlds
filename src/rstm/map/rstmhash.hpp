/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef HASH_HPP__
#define HASH_HPP__

#include "../list/rstmlist.hpp"

//#include <api/api.hpp>

// the Hash class is an array of N_BUCKETS LinkedLists
class RSTMHashTable
{
    static const int N_BUCKETS = 256;

    /**
     *  during a sanity check, we want to make sure that every element in a
     *  bucket actually hashes to that bucket; we do it by passing this
     *  method to the extendedSanityCheck for the bucket.
     */
    static bool verify_hash_function(uint32_t val, uint32_t bucket)
    {
        return ((val % N_BUCKETS) == bucket);
    }

  public:
    /**
     *  Templated type defines what kind of list we'll use at each bucket.
     */
    List bucket[N_BUCKETS];

    TM_CALLABLE
    bool insert(int val TM_ARG)
    {
        return bucket[val % N_BUCKETS].insert(val TM_PARAM);
    }

    TM_CALLABLE
    bool lookup(int val TM_ARG) const
    {
        return bucket[val % N_BUCKETS].lookup(val TM_PARAM);
    }

    TM_CALLABLE
    bool remove(int val TM_ARG)
    {
        return bucket[val % N_BUCKETS].remove(val TM_PARAM);
    }

    TM_CALLABLE
    bool update(int val TM_ARG)
    {
        return bucket[val % N_BUCKETS].update(val TM_PARAM);
    }

    bool isSane() const
    {
        for (int i = 0; i < N_BUCKETS; i++)
            if (!bucket[i].extendedSanityCheck(verify_hash_function, i))
                return false;
        return true;
    }
};

#endif // HASH_HPP__
