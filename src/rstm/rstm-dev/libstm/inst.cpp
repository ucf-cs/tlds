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
 *  This file implements the code for switching installing an algorithm.
 */

#include <sys/mman.h>
#include "inst.hpp"
#include "policies/policies.hpp"
#include "algs/algs.hpp"

namespace stm
{

  void install_algorithm_local(int new_alg, TxThread* tx)
  {
      // set my read/write/commit pointers
      tx->tmread     = stms[new_alg].read;
      tx->tmwrite    = stms[new_alg].write;
      tx->tmcommit   = stms[new_alg].commit;
  }

  /**
   *  Switch all threads to use a new STM algorithm.
   *
   *  Logically, there is an invariant that nobody is in a transaction.  This
   *  is not easy to define, though, because a thread may call this with a
   *  non-null scope, which is our "in transaction" flag.  In practice, such
   *  a thread is calling install_algorithm from the end of either its abort
   *  or commit code, so it is 'not in a transaction'
   *
   *  Another, and more important invariant, is that the caller must have
   *  personally installed begin_blocker.  There are three reasons to install
   *  begin_blocker: irrevocability, thread creation, and mode switching.
   *  Each of those actions, independently, can only be done by one thread at
   *  a time.  Furthermore, no two of those actions can be done
   *  simultaneously.
   */
  void install_algorithm(int new_alg, TxThread* tx)
  {
      // diagnostic message
      if (tx)
          printf("[%u] switching from %s to %s\n", tx->id,
                 stms[curr_policy.ALG_ID].name, stms[new_alg].name);
      if (!stms[new_alg].privatization_safe)
          printf("Warning: Algorithm %s is not privatization-safe!\n",
                 stms[new_alg].name);

      // we need to make sure the metadata remains healthy
      //
      // we do this by invoking the new alg's onSwitchTo_ method, which
      // is responsible for ensuring the invariants that are required of shared
      // and per-thread metadata while the alg is in use.
      stms[new_alg].switcher();
      CFENCE;

      // set per-thread pointers
      for (unsigned i = 0; i < threadcount.val; ++i) {
          threads[i]->tmread     = stms[new_alg].read;
          threads[i]->tmwrite    = stms[new_alg].write;
          threads[i]->tmcommit   = stms[new_alg].commit;
          threads[i]->consec_aborts  = 0;
      }

      TxThread::tmrollback = stms[new_alg].rollback;
      TxThread::tmirrevoc  = stms[new_alg].irrevoc;
      curr_policy.ALG_ID   = new_alg;
      CFENCE;
      TxThread::tmbegin    = stms[new_alg].begin;
  }

} // namespace stm
