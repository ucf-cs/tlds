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

#include "Disjoint.hpp"

/**
 *  NB: special case here, because icc doesn't like combination of noinline and
 *      header definitions.
 *
 *  do some reads only... bool return type to keep it from being optimized
 *  out
 */

bool Disjoint::ro_transaction(uint32_t id, uint32_t startpoint TM_ARG)
{
    PaddedBuffer& rBuffer =
    use_shared_read_buffer ? publicBuffer : privateBuffers[id];
    unsigned sum = 0;
    unsigned index = startpoint;
    for (unsigned i = 0; i < locations_per_transaction; i++) {
        sum += TM_READ(rBuffer.buffer[index].value);
        // compute the next index
        index = (index + 1) % DJBUFFER_SIZE;
    }
    return (sum == 0);
}



/**
 *  Step 3:
 *    Declare an instance of the data type, and provide init, test, and verify
 *    functions
 */

/*** the tree we will manipulate in the experiment */
Disjoint* SET;

/*** Initialize the disjoint buffers */
void bench_init()
{
    std::string str = CFG.bmname;
    int pos1 = str.find_first_of('-', 0);
    int pos2 = str.find_first_of('-', pos1+1);
    int pos3 = str.find_first_of('-', pos2+1);
    int pos4 = str.size();
    int size = atoi(str.substr(pos1+1, pos2-pos1-1).c_str());
    int read = atoi(str.substr(pos2+1, pos3-pos2-1).c_str());
    int write = atoi(str.substr(pos3+1, pos4-pos3-1).c_str());
    bool shared = (CFG.bmname.substr(0, 4) == "SrDw");
    SET = new Disjoint(read, write, size, shared);
}

/*** Run a bunch of random transactions */
void bench_test(uintptr_t id, uint32_t* seed)
{
   uint32_t act = rand_r_32(seed) % 100;
    // NB: volatile needed because using a non-volatile local in conjunction
    //     with a setjmp-longjmp control transfer is undefined, and gcc won't
    //     allow it with -Wall -Werror.
    volatile uint32_t start = rand_r_32(seed) % Disjoint::DJBUFFER_SIZE;

    TM_BEGIN(atomic) {
        // RO or RW transaction?
        if (act < CFG.lookpct)
            SET->ro_transaction(id, start TM_PARAM);
        else
            SET->r_rw_transaction(id, start TM_PARAM);
    } TM_END;
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
    if      (CFG.bmname == "")          CFG.bmname   = "DrDw";
}
