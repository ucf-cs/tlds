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

#include <stdint.h>
#include <iostream>
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
 *  Step 1:
 *    Include the configuration code for the harness, and the API code.
 */

/**
 *  Step 2:
 *    Declare the data type that will be stress tested via this benchmark.
 *    Also provide any functions that will be needed to manipulate the data
 *    type.  Take care to avoid unnecessary indirection.
 *
 *  NB: For the simple counter, we don't need to have an abstract data type
 */

/*** the counter we will manipulate in the experiment */
int counter;

/**
 *  Step 3:
 *    Declare an instance of the data type, and provide init, test, and verify
 *    functions
 */

/*** Initialize the counter */
void
bench_init()
{
    counter = 0;
}

/*** Run a bunch of increment transactions */
void
bench_test(uintptr_t, uint32_t*)
{
    TM_BEGIN(atomic) {
        // increment the counter
        TM_WRITE(counter, 1 + TM_READ(counter));
    } TM_END;
}

/*** Ensure the final state of the benchmark satisfies all invariants */
bool
bench_verify()
{
    std::cout << "(final value = " << counter << ") ";
    return (counter > 0);
}

/**
 *  Step 4:
 *    Include the code that has the main() function, and the code for creating
 *    threads and calling the three above-named functions.  Don't forget to
 *    provide an arg reparser.
 */

/*** no reparsing needed */
void
bench_reparse() {
    CFG.bmname = "Counter";
}
