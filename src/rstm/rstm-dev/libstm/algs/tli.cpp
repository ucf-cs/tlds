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
 *  TLI Implementation
 *
 *    This is a variant of InvalSTM.  We use 1024-bit filters, and standard
 *    "first committer wins" contention management.  What makes this algorithm
 *    interesting is that we replace all the locking from InvalSTM with
 *    optimistic mechanisms.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::timestamp;
using stm::threads;
using stm::threadcount;
using stm::WriteSetEntry;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct TLI
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
   *  TLI begin:
   */
  bool
  TLI::begin(TxThread* tx)
  {
      // mark self as alive
      tx->allocator.onTxBegin();
      tx->alive = 1;
      return false;
  }

  /**
   *  TLI commit (read-only):
   */
  void
  TLI::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // if the transaction is invalid, abort
      if (__builtin_expect(tx->alive == 2, false))
          tx->tmabort(tx);

      // ok, all is good
      tx->alive = 0;
      tx->rf->clear();
      OnReadOnlyCommit(tx);
  }

  /**
   *  TLI commit (writing context):
   */
  void
  TLI::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // if the transaction is invalid, abort
      if (__builtin_expect(tx->alive == 2, false))
          tx->tmabort(tx);

      // grab the lock to stop the world
      uintptr_t tmp = timestamp.val;
      while (((tmp&1) == 1) || (!bcasptr(&timestamp.val, tmp, (tmp+1)))) {
          tmp = timestamp.val;
          spin64();
      }

      // double check that we're valid
      if (__builtin_expect(tx->alive == 2,false)) {
          timestamp.val = tmp + 2; // release the lock
          tx->tmabort(tx);
      }

      // kill conflicting transactions
      for (uint32_t i = 0; i < threadcount.val; i++)
          if ((threads[i]->alive == 1) && (tx->wf->intersect(threads[i]->rf)))
              threads[i]->alive = 2;

      // do writeback
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));

      // release the lock and clean up
      tx->alive = 0;
      timestamp.val = tmp+2;
      tx->writes.reset();
      tx->rf->clear();
      tx->wf->clear();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  TLI read (read-only transaction)
   *
   *    We do a visible read, so we must write the fact of this read before we
   *    actually access memory.  Then, we must be sure to perform the read during
   *    a period when the world is not stopped for writeback.  Lastly, we must
   *    ensure that we are still valid
   */
  void*
  TLI::read_ro(STM_READ_SIG(tx,addr,))
  {
      // push address into read filter, ensure ordering w.r.t. the subsequent
      // read of data
      tx->rf->atomic_add(addr);

      // get a consistent snapshot of the value
      while (true) {
          uintptr_t x1 = timestamp.val;
          CFENCE;
          void* val = *addr;
          CFENCE;
          // if the ts was even and unchanged, then the read is valid
          bool ts_ok = !(x1&1) && (timestamp.val == x1);
          CFENCE;
          // if read valid, and we're not killed, return the value
          if ((tx->alive == 1) && ts_ok)
              return val;
          // abort if we're killed
          if (tx->alive == 2)
              tx->tmabort(tx);
      }
  }

  /**
   *  TLI read (writing transaction)
   */
  void*
  TLI::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      // check the log for a RAW hazard, we expect to miss
      WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));
      bool found = tx->writes.find(log);
      REDO_RAW_CHECK(found, log, mask);

      // reuse the ReadRO barrier, which is adequate here---reduces LOC
      void* val = read_ro(tx, addr STM_MASK(mask));
      REDO_RAW_CLEANUP(val, found, log, mask);
      return val;
  }

  /**
   *  TLI write (read-only context)
   */
  void
  TLI::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // buffer the write, update the filter
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      tx->wf->add(addr);
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  TLI write (writing context)
   *
   *    Just like the RO case
   */
  void
  TLI::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      tx->wf->add(addr);
  }

  /**
   *  TLI unwinder:
   */
  stm::scope_t*
  TLI::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      // clear filters and logs
      tx->rf->clear();
      if (tx->writes.size()) {
          tx->writes.reset();
          tx->wf->clear();
      }
      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  TLI in-flight irrevocability: use abort-and-restart
   */
  bool TLI::irrevoc(STM_IRREVOC_SIG(,)) { return false; }

  /**
   *  Switch to TLI:
   *
   *    Must be sure the timestamp is not odd.
   */
  void TLI::onSwitchTo()
  {
      if (timestamp.val & 1)
          ++timestamp.val;
  }
}

namespace stm {
  /**
   *  TLI initialization
   */
  template<>
  void initTM<TLI>()
  {
      // set the name
      stms[TLI].name      = "TLI";

      // set the pointers
      stms[TLI].begin     = ::TLI::begin;
      stms[TLI].commit    = ::TLI::commit_ro;
      stms[TLI].read      = ::TLI::read_ro;
      stms[TLI].write     = ::TLI::write_ro;
      stms[TLI].rollback  = ::TLI::rollback;
      stms[TLI].irrevoc   = ::TLI::irrevoc;
      stms[TLI].switcher  = ::TLI::onSwitchTo;
      stms[TLI].privatization_safe = true;
  }
}
