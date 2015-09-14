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
 *  Implement the functions that let us collect profiled transactions
 */

#include <iostream>
#include <signal.h>
#include <sys/mman.h>
#include "profiling.hpp"
#include "algs/algs.hpp"
#include "inst.hpp"

using namespace stm;

namespace
{
  /**
   *  If we change the algorithm, then we need to reset the wait and abort
   *  thresholds.  If we do not change the algorithm, then if we revisited our
   *  decision based on aborts, then backoff the wait and abort thresholds.
   */
  void adjust_thresholds(uint32_t new_alg, uint32_t old_alg)
  {
      // if we changed algs, then reset the thresholds to the policy defaults
      if (new_alg != old_alg) {
          curr_policy.waitThresh = pols[curr_policy.POL_ID].waitThresh;
          curr_policy.abortThresh = pols[curr_policy.POL_ID].abortThresh;
          return;
      }

      // if we are here because of an abort, and we didn't return yet, then we
      // did a repeat selection... double the thresholds
      if (curr_policy.abort_switch) {
          printf("Repeat Selection on Abort... backing off profile frequency\n");
          curr_policy.waitThresh *= 2;
          curr_policy.abortThresh *= 2;
          return;
      }
  }

  /**
   *  Collecting profiles is a lot like changing algorithms, but there are a
   *  few customizations we make to address the probing.
   */
  void collect_profiles(TxThread* tx)
  {
      // prevent new txns from starting
      if (!bcasptr(&TxThread::tmbegin, stms[curr_policy.ALG_ID].begin,
                   &begin_blocker))
          return;

      // wait for everyone to be out of a transaction (scope == NULL)
      for (unsigned i = 0; i < threadcount.val; ++i)
          while ((i != (tx->id-1)) && (threads[i]->scope))
              spin64();

      // remember the prior algorithm
      curr_policy.PREPROFILE_ALG = curr_policy.ALG_ID;

      // install ProfileTM
      install_algorithm(ProfileTM, tx);
  }

  /**
   *  change_algorithm is used to transition between STM implementations when
   *  ProfileTM is not involved.
   */
  void change_algorithm(TxThread* tx, unsigned new_algorithm)
  {
      // NB: we could compare new_algorithm to curr_policy.ALG_ID, and if
      //     they were the same, then we could just adjust the thresholds
      //     without doing any other work.  For now, we ignore that
      //     optimization

      // prevent new txns from starting
      if (!bcasptr(&TxThread::tmbegin, stms[curr_policy.ALG_ID].begin,
                   &begin_blocker))
          return;

      // wait for everyone to be out of a transaction (scope == NULL)
      for (unsigned i = 0; i < threadcount.val; ++i)
          while ((i != (tx->id-1)) && (threads[i]->scope))
              spin64();

      // adjust thresholds
      adjust_thresholds(new_algorithm, curr_policy.ALG_ID);

      // update the instrumentation level
      install_algorithm(new_algorithm, tx);
  }

} // (anonymous namespace)

namespace stm
{
  /*** The next CommitTrigger commit threshold */
  unsigned CommitTrigger::next = 1;

  /**
   * When a ProfileTM transaction commits, we end up in this code, which
   * calls the current policy's 'decider' to pick the new algorithm, and then
   * sets up metadata and makes the switch.
   */
  void profile_oncomplete(TxThread* tx)
  {
      // NB: This is subtle: When we switched /to/ ProfileTM, we installed
      //     begin_blocker, then changed algorithms via install_algorithm(),
      //     then uninstalled begin_blocker.  We are about to call
      //     install_algorithm(), but the invariant that install_algorithm
      //     demands is that the caller of install_algorithm() must have
      //     installed begin_blocker.  Failure to do so can result in a race
      //     with TxThread constructors.
      //
      //     No transactions can start, so we don't need to wait for everyone
      //     to commit/abort.  But we do need this, in case tmbegin is
      //     /already/ begin_blocker on account of a call to set_policy or
      //     TxThread()
      while (!bcasptr(&TxThread::tmbegin, stms[curr_policy.ALG_ID].begin,
                      &begin_blocker))
      {
          spin64();
      }


      // Use the policy to decide what algorithm to switch to
      uint32_t new_algorithm = pols[curr_policy.POL_ID].decider();

      // adjust thresholds
      adjust_thresholds(new_algorithm, curr_policy.PREPROFILE_ALG);

      // update the instrumentation level and install the algorithm
      install_algorithm(new_algorithm, tx);
  }

  void trigger_common(TxThread* tx)
  {
      // if we're dynamic, ask for profiles to be requested and then return
      if (pols[curr_policy.POL_ID].isDynamic) {
          collect_profiles(tx);
          return;
      }
      // if we're static, run the policy-specific code to decide what to do.
      // This will lead to either changing algorithms, or resetting the local
      // consec abort counter.
      uint32_t new_algorithm = pols[curr_policy.POL_ID].decider();
      change_algorithm(tx, new_algorithm);
  }
} // namespace stm

