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
 *  Serial Implementation
 *
 *    This STM is like CGL, except that we keep an undo log to support retry
 *    and restart.  Doing so requires instrumentation on writes, but not on
 *    reads.
 */

#include "../profiling.hpp"
#include "algs.hpp"

using stm::TxThread;
using stm::timestamp;
using stm::timestamp_max;
using stm::UndoLogEntry;
using stm::UNRECOVERABLE;

/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct Serial
  {
      static TM_FASTCALL bool begin(TxThread*);
      static TM_FASTCALL void* read(STM_READ_SIG(,,));
      static TM_FASTCALL void write(STM_WRITE_SIG(,,,));
      static TM_FASTCALL void commit(STM_COMMIT_SIG(,));

      static stm::scope_t* rollback(STM_ROLLBACK_SIG(,,,));
      static bool irrevoc(STM_IRREVOC_SIG(,));
      static void onSwitchTo();
  };

  /**
   *  Serial begin:
   */
  bool
  Serial::begin(TxThread* tx)
  {
      // get the lock and notify the allocator
      tx->begin_wait = tatas_acquire(&timestamp.val);
      tx->allocator.onTxBegin();
      return false;
  }

  /**
   *  Serial commit
   */
  void Serial::commit(STM_COMMIT_SIG(tx,))
  {
      // release the lock, finalize mm ops, and log the commit
      tatas_release(&timestamp.val);
      int x = tx->undo_log.size();
      tx->undo_log.reset();
      if (x)
          OnCGLCommit(tx);
      else
          OnReadOnlyCGLCommit(tx);
  }

  /**
   *  Serial read
   */
  void*
  Serial::read(STM_READ_SIG(,addr,))
  {
      return *addr;
  }

  /**
   *  Serial write
   */
  void
  Serial::write(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // add to undo log, do an in-place update
      tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
      STM_DO_MASKED_WRITE(addr, val, mask);
  }

  /**
   *  Serial unwinder:
   */
  stm::scope_t*
  Serial::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // undo all writes
      STM_UNDO(tx->undo_log, upper_stack_bound, except, len);

      // release the lock
      tatas_release(&timestamp.val);

      // reset lists
      tx->undo_log.reset();

      return PostRollback(tx);
  }

  /**
   *  Serial in-flight irrevocability:
   *
   *    NB: Since serial is protected by a single lock, we have to be really
   *        careful here.  Every transaction performs writes in-place,
   *        without per-access concurrency control.  Transactions undo-log
   *        writes to handle self-abort.  If a transaction calls
   *        'become_irrevoc', then there is an expectation that it won't
   *        self-abort, which means that we can dump its undo log.
   *
   *        The tricky part is that we can't just use the standard irrevoc
   *        framework to do this.  If T1 wants to become irrevocable
   *        in-flight, it can't wait for everyone else to finish, because
   *        they are waiting on T1.
   *
   *        The hack, for now, is to have a custom override so that on
   *        become_irrevoc, a Serial transaction clears its undo log but does
   *        no global coordination.
   */
  bool
  Serial::irrevoc(STM_IRREVOC_SIG(tx,))
  {
      UNRECOVERABLE("Serial::irrevoc should not be called!");
      return false;
  }

  /**
   *  Switch to Serial:
   *
   *    We need a zero timestamp, so we need to save its max value
   */
  void
  Serial::onSwitchTo()
  {
      timestamp_max.val = MAXIMUM(timestamp.val, timestamp_max.val);
      timestamp.val = 0;
  }
}

namespace stm
{
  /**
   *  As mentioned above, Serial needs a custom override to work with
   *  irrevocability.
   */
  void serial_irrevoc_override(TxThread* tx)
  {
      // just drop the undo log and we're good
      tx->undo_log.reset();
  }

  /**
   *  Serial initialization
   */
  template<>
  void initTM<Serial>()
  {
      // set the name
      stms[Serial].name      = "Serial";

      // set the pointers
      stms[Serial].begin     = ::Serial::begin;
      stms[Serial].commit    = ::Serial::commit;
      stms[Serial].read      = ::Serial::read;
      stms[Serial].write     = ::Serial::write;
      stms[Serial].rollback  = ::Serial::rollback;
      stms[Serial].irrevoc   = ::Serial::irrevoc;
      stms[Serial].switcher  = ::Serial::onSwitchTo;
      stms[Serial].privatization_safe = true;
  }
}
