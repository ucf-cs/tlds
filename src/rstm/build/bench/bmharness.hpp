/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef BMHARNESS_HPP__
#define BMHARNESS_HPP__

#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <pthread.h>
#include <api/api.hpp>
#include <common/platform.hpp>
#include <common/locks.hpp>
#include "bmconfig.hpp"

using std::string;
using std::cout;

Config::Config() :
    bmname(""),
    duration(1),
    execute(0),
    threads(1),
    nops_after_tx(0),
    elements(256),
    lookpct(34),
    inspct(66),
    sets(1),
    ops(1),
    time(0),
    running(true),
    txcount(0)
{
}

Config CFG TM_ALIGN(64);

/**
 * Print benchmark configuration output
 */
void dump_csv()
{
    // csv output
    std::cout << "csv"
              << ", ALG=" << GET_ALGNAME()
              << ", B=" << CFG.bmname     << ", R=" << CFG.lookpct
              << ", d=" << CFG.duration   << ", p=" << CFG.threads
              << ", X=" << CFG.execute    << ", m=" << CFG.elements
              << ", S=" << CFG.sets       << ", O=" << CFG.ops
              << ", txns=" << CFG.txcount << ", time=" << CFG.time
              << ", throughput="
              << (1000000000LL * CFG.txcount) / (CFG.time)
              << std::endl;
}

/**
 *  Print usage
 */
void usage()
{
    std::cerr << "Usage: CounterBench -C <stm algorithm> [flags]\n";
    std::cerr << "    -d: number of seconds to time (default 1)\n";
    std::cerr << "    -X: execute fixed tx count, not for a duration\n";
    std::cerr << "    -p: number of threads (default 1)\n";
    std::cerr << "    -N: nops between transactions (default 0)\n";
    std::cerr << "    -R: % lookup txns (remainder split ins/rmv)\n";
    std::cerr << "    -m: range of keys in data set\n";
    std::cerr << "    -B: name of benchmark\n";
    std::cerr << "    -S: number of sets to build (default 1)\n";
    std::cerr << "    -O: operations per transaction (default 1)\n";
    std::cerr << "    -h: print help (this message)\n\n";
}

/**
 *  Parse command line arguments
 */
void
parseargs(int argc, char** argv)
{
    // parse the command-line options
    int opt;
    while ((opt = getopt(argc, argv, "N:d:p:hX:B:m:R:S:O:")) != -1) {
        switch(opt) {
          case 'd': CFG.duration      = strtol(optarg, NULL, 10); break;
          case 'p': CFG.threads       = strtol(optarg, NULL, 10); break;
          case 'N': CFG.nops_after_tx = strtol(optarg, NULL, 10); break;
          case 'X': CFG.execute       = strtol(optarg, NULL, 10); break;
          case 'B': CFG.bmname        = std::string(optarg); break;
          case 'm': CFG.elements      = strtol(optarg, NULL, 10); break;
          case 'S': CFG.sets          = strtol(optarg, NULL, 10); break;
          case 'O': CFG.ops           = strtol(optarg, NULL, 10); break;
          case 'R':
            CFG.lookpct = strtol(optarg, NULL, 10);
            CFG.inspct = (100 - CFG.lookpct)/2 + strtol(optarg, NULL, 10);
            break;
          case 'h':
            usage();
        }
    }
}

/**
 *  Run some nops between transactions, to simulate some time being spent on
 *  computation
 */
void
nontxnwork()
{
    if (CFG.nops_after_tx)
        for (uint32_t i = 0; i < CFG.nops_after_tx; i++)
            spin64();
}

/*** Signal handler to end a test */
extern "C" void catch_SIGALRM(int) {
    CFG.running = false;
}

/**
 *  Support a few lightweight barriers
 */
void
barrier(uint32_t which)
{
    static volatile uint32_t barriers[16] = {0};
    CFENCE;
    fai32(&barriers[which]);
    while (barriers[which] != CFG.threads) { }
    CFENCE;
}

/*** Run a timed or fixed-count experiment */
void
run(uintptr_t id)
{
    // create a transactional context (repeat calls from thread 0 are OK)
    THREAD_INIT();

    // wait until all threads created, then set alarm and read timer
    barrier(0);
    if (id == 0) {
        if (!CFG.execute) {
            signal(SIGALRM, catch_SIGALRM);
            alarm(CFG.duration);
        }
        CFG.time = getElapsedTime();
    }

    // wait until read of start timer finishes, then start transactios
    barrier(1);

    uint32_t count = 0;
    uint32_t seed = id; // not everyone needs a seed, but we have to support it
    if (!CFG.execute) {
        // run txns until alarm fires
        while (CFG.running) {
            bench_test(id, &seed);
            ++count;
            nontxnwork(); // some nontx work between txns?
        }
    }
    else {
        // run fixed number of txns
        for (uint32_t e = 0; e < CFG.execute; e++) {
            bench_test(id, &seed);
            ++count;
            nontxnwork(); // some nontx work between txns?
        }
    }

    // wait until all txns finish, then get time
    barrier(2);
    if (id == 0)
        CFG.time = getElapsedTime() - CFG.time;

    // add this thread's count to an accumulator
    faa32(&CFG.txcount, count);
}

/**
 *  pthread wrapper for running the experiments
 *
 *  NB: noinline prevents this from getting inlined into main (and prevents
 *      run from being inlined there as well. This eliminates an
 *      _ITM_initializeProcess ordering problem if there's a transaction
 *      lexically scoped inside of main.
 */
NOINLINE
void*
run_wrapper(void* i)
{
    run((uintptr_t)i);
    THREAD_SHUTDOWN();
    return NULL;
}

/**
 *  Main routine: parse args, set up the TM system, prep the benchmark, run
 *  the experiments, verify results, print results, and shut down the system
 */
int main(int argc, char** argv) {
    parseargs(argc, argv);
    bench_reparse();
    SYS_INIT();
    THREAD_INIT();
    bench_init();

    void* args[256];
    pthread_t tid[256];

    // set up configuration structs for the threads we'll create
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    for (uint32_t i = 0; i < CFG.threads; i++)
        args[i] = (void*)i;

    // actually create the threads
    for (uint32_t j = 1; j < CFG.threads; j++)
        pthread_create(&tid[j], &attr, &run_wrapper, args[j]);

    // all of the other threads should be queued up, waiting to run the
    // benchmark, but they can't until this thread starts the benchmark
    // too...
    run_wrapper(args[0]);

    // Don't call any more transactional functions, because run_wrapper called
    // shutdown.

    // everyone should be done.  Join all threads so we don't leave anything
    // hanging around
    for (uint32_t k = 1; k < CFG.threads; k++)
        pthread_join(tid[k], NULL);

    bool v = bench_verify();
    std::cout << "Verification: " << (v ? "Passed" : "Failed") << "\n";

    dump_csv();

    // And call sys shutdown stuff
    SYS_SHUTDOWN();
    return 0;
}

#endif // BMHARNESS_HPP__
