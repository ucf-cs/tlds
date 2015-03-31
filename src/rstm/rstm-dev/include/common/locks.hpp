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
 *  Given all the atomic operations we defined in platform.hpp, we can now
 *  declare the lock types that we need for implementing some single-lock
 *  based STMs.
 */

#ifndef LOCKS_HPP__
#define LOCKS_HPP__

#include "common/platform.hpp"

/**
 *  Tune backoff parameters
 *
 *  NB: At some point (probably mid 2010), these values were experimentally
 *      verified to provide good performance for some workload using TATAS
 *      locks.  Whether they are good values anymore is an open question.
 */
#if defined(STM_CPU_SPARC)
#define MAX_TATAS_BACKOFF 4096
#else // (STM_CPU_X86)
#define MAX_TATAS_BACKOFF 524288
#endif

/***  Issue 64 nops to provide a little busy waiting */
inline void spin64()
{
    for (int i = 0; i < 64; i++)
        nop();
}

/***  exponential backoff for TATAS locks */
inline void backoff(int *b)
{
    for (int i = *b; i; i--)
        nop();
    if (*b < MAX_TATAS_BACKOFF)
        *b <<= 1;
}

/***  TATAS lock: test-and-test-and-set with exponential backoff */
typedef volatile uintptr_t tatas_lock_t;

/***  Slowpath TATAS acquire.  This performs exponential backoff */
inline int tatas_acquire_slowpath(tatas_lock_t* lock)
{
    int b = 64;
    do {
        backoff(&b);
    } while (tas(lock));
    return b;
}

/**
 *  Fastpath TATAS acquire.  The return value is how long we spent spinning
 */
inline int tatas_acquire(tatas_lock_t* lock)
{
    if (!tas(lock))
        return 0;
    return tatas_acquire_slowpath(lock);
}

/***  TATAS release: ordering is safe for SPARC, x86 */
inline void tatas_release(tatas_lock_t* lock)
{
    CFENCE;
    *lock = 0;
}

/**
 *  Ticket lock: this is the easiest implementation possible, but might not be
 *  the most optimal w.r.t. cache misses
 */
struct ticket_lock_t
{
    volatile uintptr_t next_ticket;
    volatile uintptr_t now_serving;
};

/**
 *  Acquisition of a ticket lock entails an increment, then a spin.  We use a
 *  counter to measure how long we spend spinning, in case that information
 *  is useful to adaptive STM mechanisms.
 */
inline int ticket_acquire(ticket_lock_t* lock)
{
    int ret = 0;
    uintptr_t my_ticket = faiptr(&lock->next_ticket);
    while (lock->now_serving != my_ticket)
        ret++;
    return ret;
}

/***  Release the ticket lock */
inline void ticket_release(ticket_lock_t* lock)
{
    lock->now_serving += 1;
}

/***  Simple MCS lock implementation */
struct mcs_qnode_t
{
    volatile bool flag;
    volatile mcs_qnode_t* volatile next;
};

/**
 *  MCS acquire.  We count how long we spin, in order to detect very long
 *  delays
 */
inline int mcs_acquire(mcs_qnode_t** lock, mcs_qnode_t* mine)
{
    // init my qnode, then swap it into the root pointer
    mine->next = 0;
    mcs_qnode_t* pred = (mcs_qnode_t*)atomicswapptr(lock, mine);

    // now set my flag, point pred to me, and wait for my flag to be unset
    int ret = 0;
    if (pred != 0) {
        mine->flag = true;
        pred->next = mine;
        while (mine->flag) { ret++; } // spin
    }
    return ret;
}

/***  MCS release */
inline void mcs_release(mcs_qnode_t** lock, mcs_qnode_t* mine)
{
    // if my node is the only one, then if I can zero the lock, do so and I'm
    // done
    if (mine->next == 0) {
        if (bcasptr(lock, mine, static_cast<mcs_qnode_t*>(NULL)))
            return;
        // uh-oh, someone arrived while I was zeroing... wait for arriver to
        // initialize, fall out to other case
        while (mine->next == 0) { } // spin
    }
    // other case: someone is waiting on me... set their flag to let them start
    mine->next->flag = false;
}

#endif // LOCKS_HPP__
