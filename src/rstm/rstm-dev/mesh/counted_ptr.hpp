/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef COUNTED_PTR_HPP__
#define COUNTED_PTR_HPP__

#include "common/platform.hpp"

//  Counted pointers incorporate a serial number to avoid the A-B-A problem
//  in shared data structures.  They are needed for algorithms based on
//  compare-and-swap.  We could get by without them if we had LL/SC.
//
union counted_ptr
{
    struct {
        void* volatile ptr;
        volatile unsigned long sn;      // serial number
    } p;
    volatile unsigned long long all;
        // For when we need to read the whole thing at once.
        // (We're assuming that doubles are loaded and stored with a single
        // atomic instruction)
} __attribute__ ((aligned(8)));

static inline bool
cp_cas(volatile unsigned long long* addr,
     void* expected_ptr, unsigned long expected_sn,
     void* new_ptr)
{
    unsigned long new_sn = expected_sn + 1;
    counted_ptr old_cp;
    old_cp.p.ptr = expected_ptr;  old_cp.p.sn = expected_sn;
    counted_ptr new_cp;
    new_cp.p.ptr = new_ptr;  new_cp.p.sn = new_sn;;
    return __sync_bool_compare_and_swap(addr, old_cp.all, new_cp.all);
}

#endif // COUNTED_PTR_HPP__
