/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/**
 *  Step 1:
 *    Include the configuration code for the harness, and the API code.
 */

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
 *  Step 2:
 *    Declare the data type that will be stress tested via this benchmark.
 *    Also provide any functions that will be needed to manipulate the data
 *    type.  Take care to avoid unnecessary indirection.
 */

#include "DList.hpp"



/**
 *  Step 3:
 *    Declare an instance of the data type, and provide init, test, and verify
 *    functions
 */

/*** the list we will manipulate in the experiment */
DList* list;

/*** Initialize the list */
void bench_init()
{
    list = new DList();
    // populate list with all values from 0 to LIVELOCK_ELEMENTS - 1
    TM_BEGIN_FAST_INITIALIZATION();
    for (uint32_t i = 0; i < CFG.elements; i++)
        list->insert(i TM_PARAM);
    TM_END_FAST_INITIALIZATION();
}

/**
 * Run a bunch of transactions that should cause conflicts
 * threads either increment from front to back or from back to front,
 * based on ID.  This creates lots of conflicts
 */
void bench_test(uintptr_t id, uint32_t*)
{
    TM_BEGIN(atomic) {
        // need to look at the timer, or we'll livelock!
        if (CFG.running) {
            if (id % 2)
                list->increment_forward(TM_PARAM_ALONE);
            else
                list->increment_backward(TM_PARAM_ALONE);
        }
    } TM_END;
}

/*** Ensure the final state of the benchmark satisfies all invariants */
bool bench_verify() { return list->isSane(); }

/**
 *  Step 4:
 *    Include the code that has the main() function, and the code for creating
 *    threads and calling the three above-named functions.  Don't forget to
 *    provide an arg reparser.
 */

/*** Deal with special names that map to different M values */
void bench_reparse()
{
    if (CFG.bmname == "") CFG.bmname = "WWPathology";
}
