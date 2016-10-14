/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef LEHIGH_H__
#define LEHIGH_H__

#include "pair.h"

/**
 * STAMP does some odd things with function pointers, which makes g++ unhappy
 * and makes it very hard to reason about the correctness of code.  The most
 * offensive is that STAMP may coerce a function pointer for a 3-argument
 * comparison function into a 2-argument comparison function.
 *
 * We avoid this situation by saying that rather than store a function pointer
 * within a collection's root node, we'll store a pointer to a struct, where
 * the struct has two function pointers: one for transactional code, one for
 * nontransactional code, with 3 and 2 parameters, respectively.  This adds a
 * level of indirection (though only once per collection traversal, if we are
 * careful), but increases safety and allows better reasoning.
 */
typedef struct comparator
{
    /*** two functions that return {-1,0,1}, where 0 -> equal ***/
    /*** the non-transactional version */
    union {
        long int (*compare_notm)(const void*, const void*);
        long int (*compare_pair_notm)(const pair_t*, const pair_t*);
    };

    /*** the transactional version */
    union {
        TM_CALLABLE long int (*compare_tm)(TM_ARGDECL const void*, const void*);
        TM_CALLABLE long int (*compare_pair_tm)(TM_ARGDECL const pair_t*, const pair_t*);
    };

    /*** the unions necessitate use of constructors :( */
    comparator (long int (*c1)(const void*, const void*),
                TM_CALLABLE long int (*c2)(TM_ARGDECL const void*, const void*))
    {
        compare_notm = c1;
        compare_tm = c2;
    }
    comparator (long int (*c1)(const pair_t*, const pair_t*),
                TM_CALLABLE long int (*c2)(TM_ARGDECL const pair_t*, const pair_t*))
    {
        compare_pair_notm = c1;
        compare_pair_tm = c2;
    }

} comparator_t;

#endif // LEHIGH_H__
