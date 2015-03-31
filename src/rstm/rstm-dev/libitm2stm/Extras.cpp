/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include "stm/txthread.hpp"
#include "Extras.h"

// It's important to allow users to declare malloc, free, new, and delete to be
// [[transaction_safe]]. Intel's itm2stm actually contains transactional versions
// of these functions. We'll also provide transactional versions, mapping them
// to RSTM's internal allocation routines.
//
// We use the asm declarations to make sure that they get the "right"
// symbols. This WON'T WORK if the user tries to call one of these routines
// through a function pointer inside of a transaction, because the standard
// transactional calling convention relies on co-location of transactional and
// non-transactional functions, with a non-transactional entry point that
// contains the address of the transactional symbol.
//
// If we wanted to make function pointers work, we'd have to require that the
// library builder compile with icc (or at least compile this file with icc),
// and declare the functions using icc's [[tm_wrapping()]] extension.

/* Wrap: malloc operator(std::size_t sz)  */
void*
itm2stm::malloc_wrapper(size_t sz)
{
    return stm::Self->allocator.txAlloc(sz);
}

/* Wrap: free operator(std::size_t sz)  */
void
itm2stm::free_wrapper(void *ptr)
{
    stm::Self->allocator.txFree(ptr);
}


/* Wrap: operator new (std::size_t sz)  */
void*
itm2stm::new_wrapper(size_t sz)
{
    return stm::Self->allocator.txAlloc(sz);
}


/* Wrap: operator delete (void *ptr)  */
void
itm2stm::delete_wrapper(void *ptr)
{
    stm::Self->allocator.txFree(ptr);
}
