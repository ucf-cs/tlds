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
#include <vector>
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

#include "Tree.hpp"



/**
 *  Forest of RBTrees
 *
 *    In the past, we made this have lots of different sized trees.  That, in
 *    turn, led to a lot of complexity, and resulted in the benchmark not
 *    receiving much use.  In this new instance, we make things less
 *    configurable, but hopefully more useful.
 *
 *    The Forest holds many trees, all of which are exactly the same wrt key
 *    range and lookup ratio.  A transaction performs a fixed number of tree
 *    ops on the forest, selecting trees at random (with replacement).
 */
struct CustomForest
{
    uint32_t keydepths;             // how many keys in each tree?
    uint32_t roratio;               // lookup ratio for each tree
    uint32_t insratio;              // insert ratio for each tree
    uint32_t total_trees;           // total number of trees
    uint32_t trees_per_tx;          // random trees to touch per transaction
    RBTree** trees;                 // set of trees

    // constructor: create all the trees, intiialize them
    CustomForest(uint32_t keys, uint32_t ro, uint32_t numtrees, uint32_t per)
        : keydepths(keys), roratio(ro), insratio(ro + (100 - ro)/2),
          total_trees(numtrees), trees_per_tx(per), trees(new RBTree*[numtrees])
    {
        // initialize the data structure
        TM_BEGIN_FAST_INITIALIZATION();
        for (uint32_t i = 0; i < numtrees; ++i) {
            // make new tree
            trees[i] = new RBTree();
            // populate it
            for (uint32_t w = 0; w < keys; w+=2) {
                // NB: gross hack: we can cheat here to avoid
                // instrumentation.  it's safe since we're in the
                // constructor, but not advised in general.
                trees[i]->insert(w TM_PARAM);
            }
        }
        TM_END_FAST_INITIALIZATION();
    }

    ~CustomForest() {
        for (uint32_t i = 0; i < total_trees; ++i)
            delete trees[i];
        delete[] trees;
    }
};

/**
 *  Step 3:
 *    Declare an instance of the data type, and provide init, test, and verify
 *    functions
 */

/*** the tree we will manipulate in the experiment */
CustomForest* SET;

/*** Initialize the counter */
void bench_init()
{
    SET = new CustomForest(CFG.elements, CFG.lookpct, CFG.sets, CFG.ops);
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
        if (CFG.running) {
            local_seed = *seed;
            for (uint32_t i = 0; i < SET->trees_per_tx; ++i) {
                // pick a tree, a value, and a read-only ratio
                int tree_idx = rand_r_32(&local_seed) % SET->total_trees;
                int val = rand_r_32(&local_seed) % SET->keydepths;
                uint32_t act = rand_r_32(&local_seed) % 100;
                // do a lookup?
                if (act < SET->roratio)
                    SET->trees[tree_idx]->lookup(val TM_PARAM);
                else if (act < SET->insratio)
                    SET->trees[tree_idx]->insert(val TM_PARAM);
                else
                    SET->trees[tree_idx]->remove(val TM_PARAM);
            }
        }
    } TM_END;
    *seed = local_seed;
}

/*** Ensure the final state of the benchmark satisfies all invariants */
bool bench_verify()
{
    for (uint32_t i = 0; i < SET->total_trees; ++i)
        if (!(SET->trees[i]->isSane()))
            return false;
    return true;
}

/**
 *  Step 4:
 *    Include the code that has the main() function, and the code for creating
 *    threads and calling the three above-named functions.  Don't forget to
 *    provide an arg reparser.
 */

/*** Deal with special names that map to different M values */
void bench_reparse()
{
    if (CFG.bmname == "") CFG.bmname   = "Forest";
}
