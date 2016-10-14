/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <stm/config.h>
#if defined(STM_CPU_SPARC)
#include <sys/types.h>
#endif

/**
 *  Step 1:
 *    Include the configuration code for the harness, and the API code.
 */
#include <iostream>
#include <alt-license/rand_r_32.h>
#include <api/api.hpp>
#include "bmconfig.hpp"

/**
 *  We provide the option to build the entire benchmark in a single
 *  source. The bmconfig.hpp include defines all of the important functions
 *  that are implemented in this file, and bmharness.cpp defines the
 *  execution infrastructure.
 */
#ifdef SINGLE_SOURCE_BUILD
#include "bmharness.cpp"
#endif

/**
 *  Step 2:
 *    Declare the data type that will be stress tested via this benchmark.
 *    Also provide any functions that will be needed to manipulate the data
 *    type.  Take care to avoid unnecessary indirection.
 */

int* matrix;

/**
 *  Step 3:
 *    Declare an instance of the data type, and provide init, test, and verify
 *    functions
 */

/*** Initialize an array that we can use for our MCAS test */
void bench_init()
{
    matrix = (int*)malloc(CFG.elements * sizeof(int));
}

/*** Run a bunch of random transactions */
void bench_test(uintptr_t, uint32_t* seed)
{
    // cache the seed locally so we can restore it on abort
    //
    // NB: volatile needed because using a non-volatile local in conjunction
    //     with a setjmp-longjmp control transfer is undefined, and gcc won't
    //     allow it with -Wall -Werror.
    volatile uint32_t local_seed = *seed;

    TM_BEGIN(atomic) {
        for (uint32_t i = 0; i < CFG.ops; ++i) {
            uint32_t loc = rand_r_32(&local_seed) % CFG.elements;
            TM_WRITE(matrix[loc], 1 + TM_READ(matrix[loc]));
        }
    } TM_END;
    *seed = local_seed;
}

/*** Ensure the final state of the benchmark satisfies all invariants */
bool bench_verify() { return true; }

/**
 *  Step 4:
 *    Include the code that has the main() function, and the code for creating
 *    threads and calling the three above-named functions.  Don't forget to
 *    provide an arg reparser.
 */

/*** Deal with special names that map to different M values */
void bench_reparse()
{
    if      (CFG.bmname == "")          CFG.bmname   = "MCAS";
}
