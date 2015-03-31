/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include "common/platform.hpp"   // NORETURN, FASTCALL, etc
#include "stm/lib_globals.hpp"   // AbortHandler
#include "stm/macros.hpp"        // barrier signatures
#include "stm/txthread.hpp"      // TxThread stuff
#include "policies/policies.hpp" // curr_policy
#include "algs/algs.hpp"         // stms
#include "algs/tml_inline.hpp"

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::AbortHandler;
using stm::stms;
using stm::curr_policy;
using stm::CGL;

namespace {
  /**
   *  The abort handler is set during sys_init. We overwrite it with
   *  abort_irrevocable when a transaction becomes irrevocable, and we save the
   *  old one here so we can restore it during commit.
   */
  AbortHandler old_abort_handler = NULL;

  /**
   *  Handler for abort attempts while irrevocable. Useful for trapping problems
   *  early.
   */
  NORETURN void abort_irrevocable(TxThread* tx)
  {
      UNRECOVERABLE("Irrevocable thread attempted to abort.");
  }

  /**
   *  Handler for rollback attempts while irrevocable. Useful for trapping
   *  problems early.
   *
   *  NB: For whatever reason a 'using stm::scope_t' triggers an ICE in Mac OS
   *      X's default gcc-4.2.1. It's fine if we use the fully qualified
   *      namespace here.
   */
  stm::scope_t* rollback_irrevocable(STM_ROLLBACK_SIG(,,,))
  {
      UNRECOVERABLE("Irrevocable thread attempted to rollback.");
      return NULL;
  }

  /**
   *  Resets all of the barriers to be the curr_policy bariers, except for
   *  tmabort which reverts to the one we saved, and tmbegin which should be
   *  done manually in the caller.
   */
  inline void unset_irrevocable_barriers(TxThread& tx)
  {
      tx.tmread           = stms[curr_policy.ALG_ID].read;
      tx.tmwrite          = stms[curr_policy.ALG_ID].write;
      tx.tmcommit         = stms[curr_policy.ALG_ID].commit;
      tx.tmrollback       = stms[curr_policy.ALG_ID].rollback;
      TxThread::tmirrevoc = stms[curr_policy.ALG_ID].irrevoc;
      tx.tmabort          = old_abort_handler;
  }

  /**
   *  custom commit for irrevocable transactions
   */
  TM_FASTCALL void commit_irrevocable(STM_COMMIT_SIG(tx,))
  {
      // make self non-irrevocable, and unset local r/w/c barriers
      tx->irrevocable = false;
      unset_irrevocable_barriers(*tx);
      // now allow other transactions to run
      CFENCE;
      TxThread::tmbegin = stms[curr_policy.ALG_ID].begin;
      // finally, call the standard commit cleanup routine
      OnReadOnlyCommit(tx);
  }

  /**
   *  Sets all of the barriers to be irrevocable, except tmbegin.
   */
  inline void set_irrevocable_barriers(TxThread& tx)
  {
      tx.tmread           = stms[CGL].read;
      tx.tmwrite          = stms[CGL].write;
      tx.tmcommit         = commit_irrevocable;
      tx.tmrollback       = rollback_irrevocable;
      TxThread::tmirrevoc = stms[CGL].irrevoc;
      old_abort_handler   = tx.tmabort;
      tx.tmabort          = abort_irrevocable;
  }
}

namespace stm
{
  /**
   *  The 'Serial' algorithm requires a custom override for irrevocability,
   *  which we implement here.
   */
  void serial_irrevoc_override(TxThread* tx);

  /**
   *  Try to become irrevocable, inflight. This happens via mode
   *  switching. If the inflight irrevocability fails, we fall-back to an
   *  abort-and-restart-as-irrevocable scheme, based on the understanding
   *  that the begin_blocker tmbegin barrier will configure us as irrevocable
   *  and let us through if we have our irrevocable flag set. In addition to
   *  letting us through, it will set our barrier pointers to be the
   *  irrevocable barriers---it has to be done here because the rollback that
   *  the abort triggers will reset anything we try and set here.
   */
  void become_irrevoc(STM_WHEN_PROTECT_STACK(void** upper_stack_bound))
  {
      TxThread* tx = Self;
      // special code for degenerate STM implementations
      //
      // NB: stm::is_irrevoc relies on how this works, so if it changes then
      //     please update that code as well.
      if (TxThread::tmirrevoc == stms[CGL].irrevoc)
          return;

      if ((curr_policy.ALG_ID == MCS) || (curr_policy.ALG_ID == Ticket))
          return;

      if (curr_policy.ALG_ID == Serial) {
          serial_irrevoc_override(tx);
          return;
      }

      if (curr_policy.ALG_ID == TML) {
          if (!tx->tmlHasLock)
              beforewrite_TML(tx);
          return;
      }

      // prevent new txns from starting.  If this fails, it means one of
      // three things:
      //
      //  - Someone else became irrevoc
      //  - Thread creation is in progress
      //  - Adaptivity is in progress
      //
      //  The first of these cases requires us to abort, because the irrevoc
      //  thread is running the 'wait for everyone' code that immediately
      //  follows this CAS.  Since we can't distinguish the three cases,
      //  we'll just abort all the time.  The impact should be minimal.
      if (!bcasptr(&TxThread::tmbegin, stms[curr_policy.ALG_ID].begin,
                   &begin_blocker))
          tx->tmabort(tx);

      // wait for everyone to be out of a transaction (scope == NULL)
      for (unsigned i = 0; i < threadcount.val; ++i)
          while ((i != (tx->id-1)) && (threads[i]->scope))
              spin64();

      // try to become irrevocable inflight (protects the stack during commit if
      // necessary)
#if defined(STM_PROTECT_STACK)
      tx->irrevocable = TxThread::tmirrevoc(tx, upper_stack_bound);
#else
      tx->irrevocable = TxThread::tmirrevoc(tx);
#endif

      // If inflight succeeded, switch our barriers and return true.
      if (tx->irrevocable) {
          set_irrevocable_barriers(*tx);
          return;
      }

      // Otherwise we tmabort (but mark ourselves as irrevocable so that we get
      // through the begin_blocker after the abort). We don't switch the barriers
      // here because a) one of the barriers that we'd like to switch is
      // rollback, which is used by tmabort and b) rollback is designed to reset
      // our barriers to the default read-only barriers for the algorithm which
      // will just overwrite what we do here
      //
      // begin_blocker sets our barriers to be irrevocable if we have our
      // irrevocable flag set.
      tx->irrevocable = true;
      tx->tmabort(tx);
  }

  /**
   * True if the current algorithm is irrevocable.
   */
  bool is_irrevoc(const TxThread& tx)
  {
      if (tx.irrevocable || TxThread::tmirrevoc == stms[CGL].irrevoc)
          return true;
      if ((curr_policy.ALG_ID == MCS) || (curr_policy.ALG_ID  == Ticket))
          return true;
      if ((curr_policy.ALG_ID == TML) && (tx.tmlHasLock))
          return true;
      if (curr_policy.ALG_ID == Serial)
          return true;
      return false;
  }

  /**
   *  Custom begin method that blocks the starting thread, in order to get
   *  rendezvous correct during mode switching and GRL irrevocability. It
   *  doubles as an irrevocability mechanism for implementations where we don't
   *  have (or can't write) an in-flight irrevocability mechanism.
   */
  bool begin_blocker(TxThread* tx)
  {
      if (tx->irrevocable) {
          set_irrevocable_barriers(*tx);
          return true;
      }

      // adapt without longjmp
      while (true) {
          // first, clear the outer scope, because it's our 'tx/nontx' flag
          scope_t* b = tx->scope;
          tx->scope = 0;
          // next, wait for the begin_blocker to be uninstalled
          while (TxThread::tmbegin == begin_blocker)
              spin64();
          CFENCE;
          // now re-install the scope
#ifdef STM_CPU_SPARC
          tx->scope = b; WBR;
#else
          casptr((volatile uintptr_t*)&tx->scope, (uintptr_t)0, (uintptr_t)b);
#endif
          // read the begin function pointer AFTER setting the scope
          bool TM_FASTCALL (*beginner)(TxThread*) = TxThread::tmbegin;
          // if begin_blocker is no longer installed, we can call the pointer
          // to start a transaction, and then return.  Otherwise, we missed our
          // window, so we need to go back to the top of the loop.
          if (beginner != begin_blocker)
              return beginner(tx);
      }
  }
}

