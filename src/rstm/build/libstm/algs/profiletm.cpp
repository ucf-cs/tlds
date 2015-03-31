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
 *  ProfileTM Implementation
 *
 *    To figure out what a workload looks like, the runtime can switch to
 *    this STM.  It enforces serial execution, and runs each transaction in a
 *    manner that is profiled for common high-level features, such as #
 *    reads/writes, tx time, etc.  This STM should never be selected
 *    directly.
 */

#include "../profiling.hpp"
#include "algs.hpp"

using namespace stm;

namespace
{
/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
  struct ProfileTM
  {
      static TM_FASTCALL bool begin(TxThread*);
      static TM_FASTCALL void* read_ro(STM_READ_SIG(,,));
      static TM_FASTCALL void* read_rw(STM_READ_SIG(,,));
      static TM_FASTCALL void write_ro(STM_WRITE_SIG(,,,));
      static TM_FASTCALL void write_rw(STM_WRITE_SIG(,,,));
      static TM_FASTCALL void commit_ro(STM_COMMIT_SIG(,));
      static TM_FASTCALL void commit_rw(STM_COMMIT_SIG(,));

      static stm::scope_t* rollback(STM_ROLLBACK_SIG(,,,));
      static bool irrevoc(STM_IRREVOC_SIG(,));
      static void onSwitchTo();
  };

  /**
   *  ProfileTM begin:
   *
   *    We use a ticket lock so that we can run N consecutive transactions,
   *    but allowing any thread to participate in the set of transactions
   *    that we run.
   */
  bool
  ProfileTM::begin(TxThread* tx)
  {
      // bump the last_init field
      uintptr_t my_order = faiptr(&last_init.val);
      // if my order can fit in a range from 0 .. profile_txns - 1, wait my
      // turn
      if (my_order < profile_txns) {
          while (last_complete.val < my_order) spin64();
          // OK, I have the ticket.  Go for it!
          // update allocator
          tx->allocator.onTxBegin();
          // clear the profile buffer I'll be filling
          profiles[last_complete.val].clear();
          // record the start time, so we can compute duration
          profiles[last_complete.val].txn_time = tick();
          return false;
      }

      // uh-oh, we can't fit this transaction into the range... act like
      // we're in begin_blocker, but don't resume until we're neither in
      // begin_blocker nor begin().
      while (true) {
          // first, clear the outer scope, because it's our 'tx/nontx' flag
          stm::scope_t* b = tx->scope;
          tx->scope = 0;
          // next, wait for a good begin pointer
          while ((TxThread::tmbegin == begin) ||
                 (TxThread::tmbegin == stm::begin_blocker))
              spin64();
          CFENCE;
          // now reinstall the scope
#ifdef STM_CPU_SPARC
          tx->scope = b; WBR;
#else
          casptr((volatile uintptr_t*)&tx->scope, (uintptr_t)0, (uintptr_t)b);
#endif
          // read the begin function pointer AFTER setting the scope
          bool TM_FASTCALL (*beginner)(TxThread*) = TxThread::tmbegin;
          // if begin_blocker is no longer installed, and ProfileTM::begin
          // isn't installed either, we can call the pointer to start a
          // transaction, and then return.  Otherwise, we missed our window,
          // so we need to go back to the top of the loop
          if ((beginner != stm::begin_blocker) && (beginner != begin))
              return beginner(tx);
      }
  }

  /**
   *  ProfileTM commit (read-only):
   *
   *    To commit, we update the statistics and release the ticket lock.
   *    From here, we might also invoke the algorithm selector, if this is
   *    the final transaction of the set that were requested.
   */
  void
  ProfileTM::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // figure out this transaction's running time
      unsigned long long tmp = tick();
      profiles[last_complete.val].txn_time =
          tmp - profiles[last_complete.val].txn_time;

      // do all the standard RO cleanup stuff
      OnReadOnlyCommit(tx);

      // now adapt based on the fact that we just successfully collected a
      // profile
      if (++last_complete.val == profile_txns)
          profile_oncomplete(tx);
  }

  /**
   *  ProfileTM commit (writing context):
   *
   *    Same as RO case, but we must also perform the writeback.
   */
  void
  ProfileTM::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // we're committed... run the redo log, remember that this is a commit
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
      int x = tx->writes.size();
      tx->writes.reset();

      // figure out this transaction's running time
      unsigned long long tmp = tick();
      profiles[last_complete.val].txn_time =
          tmp - profiles[last_complete.val].txn_time;

      // log the distinct writes, so we can compute the WAW count
      profiles[last_complete.val].write_nonwaw = x;
      profiles[last_complete.val].write_waw -= x;

      // do all the standard RW cleanup stuff
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);

      // now adapt based on the fact that we just successfully collected a
      // profile
      if (++last_complete.val == profile_txns)
          profile_oncomplete(tx);
  }

  /**
   *  ProfileTM read (read-only transaction)
   *
   *    Simply read the location, and remember that we did a read
   */
  void*
  ProfileTM::read_ro(STM_READ_SIG(,addr,))
  {
      ++profiles[last_complete.val].read_ro;
      return *addr;
  }

  /**
   *  ProfileTM read (writing transaction)
   */
  void*
  ProfileTM::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      // check the log
      WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));
      if (tx->writes.find(log)) {
          ++profiles[last_complete.val].read_rw_raw;
          return log.val;
      }

      ++profiles[last_complete.val].read_rw_nonraw;
      return *addr;
  }

  /**
   *  ProfileTM write (read-only context)
   */
  void
  ProfileTM::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // do a buffered write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      ++profiles[last_complete.val].write_waw;
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  ProfileTM write (writing context)
   */
  void
  ProfileTM::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // do a buffered write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      ++profiles[last_complete.val].write_waw;
  }

  /**
   *  ProfileTM unwinder:
   *
   *    In the off chance that the profiled transaction must abort, we call
   *    this to wrap up.
   *
   *    NB: This code has not been tested in a while
   */
  stm::scope_t*
  ProfileTM::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      // finish the profile
      profiles[last_complete.val].txn_time =
          tick() - profiles[last_complete.val].txn_time;

      // clean up metadata
      tx->writes.reset();

      // NB: This is subtle.  In ProfileTM, N transactions run one at a time.
      //     They each run once.  So if this transaction aborts, then we need
      //     to treat it like a completed transaction, from the perspective
      //     of the profiling mechanism, so that some other transaction can
      //     be selected for execution.  However, if this transaction was the
      //     /last/ transaction in the profile set, then right now we need to
      //     do a profile_oncomplete to pick a new algorithm, even though the
      //     transaction aborted.
      //
      //    The problem is that profile_oncomplete is going to call
      //    install_algorithm, which will change all per-thread pointers.
      //    But then we are going to rollback this transaction, and
      //    PostRollback must sometimes also reset the per-thread pointers to
      //    ProfileTM's read/write/commit
      //
      //    The most orthogonal solution, which isn't very orthogonal, is to
      //    have two different PostRollbackNoTrigger calls: one resets the
      //    pointers, the other doesn't.  The one we pick depends on whether
      //    we call profile_oncomplete() or not.
      if (++last_complete.val == profile_txns) {
          profile_oncomplete(tx);
          return PostRollbackNoTrigger(tx);
      }
      return PostRollbackNoTrigger(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  ProfileTM in-flight irrevocability:
   *
   *    NB: For now, if a ProfileTM txn wants to go irrevocable, we will
   *        crash the application.  That's not a good long-term plan
   */
  bool
  ProfileTM::irrevoc(STM_IRREVOC_SIG(,))
  {
      UNRECOVERABLE("Irrevocable ProfileTM transactions are not supported");
      return false;
  }

  /**
   *  Switch to ProfileTM:
   *
   *    To support multiple threads probing, we use last_init and
   *    last_complete as a ticket lock, so they need to be zero
   */
  void
  ProfileTM::onSwitchTo()
  {
      last_init.val = 0;
      last_complete.val = 0;
  }
}

namespace stm
{
  /**
   *  ProfileTM initialization
   */
  template<>
  void initTM<ProfileTM>()
  {
      // set the name
      stms[ProfileTM].name      = "ProfileTM";

      // set the pointers
      stms[ProfileTM].begin     = ::ProfileTM::begin;
      stms[ProfileTM].commit    = ::ProfileTM::commit_ro;
      stms[ProfileTM].read      = ::ProfileTM::read_ro;
      stms[ProfileTM].write     = ::ProfileTM::write_ro;
      stms[ProfileTM].rollback  = ::ProfileTM::rollback;
      stms[ProfileTM].irrevoc   = ::ProfileTM::irrevoc;
      stms[ProfileTM].switcher  = ::ProfileTM::onSwitchTo;
      stms[ProfileTM].privatization_safe = true;
  }
}
