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
 *  TMLLazy Implementation
 *
 *    This is just like TML, except that we use buffered update and we wait to
 *    become the 'exclusive writer' until commit time.  The idea is that this
 *    is supposed to increase concurrency, and also that this should be quite
 *    fast even though it has the function call overhead.  This algorithm
 *    provides at least ALA semantics.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::TxThread;
using stm::timestamp;
using stm::WriteSetEntry;

/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.  Note that with TML, we don't expect the reads and
 *  writes to be called, because we expect the isntrumentation to be inlined
 *  via the dispatch mechanism.  However, we must provide the code to handle
 *  the uncommon case.
 */
namespace {
  struct TMLLazy {
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
   *  TMLLazy begin:
   */
  bool
  TMLLazy::begin(TxThread* tx)
  {
      // Sample the sequence lock until it is even (unheld)
      while ((tx->start_time = timestamp.val)&1)
          spin64();

      // notify the allocator
      tx->allocator.onTxBegin();
      return false;
  }

  /**
   *  TMLLazy commit (read-only context):
   */
  void
  TMLLazy::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // no metadata to manage, so just be done!
      OnReadOnlyCommit(tx);
  }

  /**
   *  TMLLazy commit (writer context):
   */
  void
  TMLLazy::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // we have writes... if we can't get the lock, abort
      if (!bcasptr(&timestamp.val, tx->start_time, tx->start_time + 1))
          tx->tmabort(tx);

      // we're committed... run the redo log
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));

      // release the sequence lock and clean up
      timestamp.val++;
      tx->writes.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  TMLLazy read (read-only context)
   */
  void*
  TMLLazy::read_ro(STM_READ_SIG(tx,addr,))
  {
      // read the actual value, direct from memory
      void* tmp = *addr;
      CFENCE;

      // if the lock has changed, we must fail
      //
      // NB: this form of /if/ appears to be faster
      if (__builtin_expect(timestamp.val == tx->start_time, true))
          return tmp;
      tx->tmabort(tx);
      // unreachable
      return NULL;
  }

  /**
   *  TMLLazy read (writing context)
   */
  void*
  TMLLazy::read_rw(STM_READ_SIG(tx,addr,mask))
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
   *  TMLLazy write (read-only context):
   */
  void
  TMLLazy::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // do a buffered write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  TMLLazy write (writing context):
   */
  void
  TMLLazy::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // do a buffered write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
  }

  /**
   *  TMLLazy unwinder
   */
  stm::scope_t*
  TMLLazy::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);
      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      tx->writes.reset();
      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  TMLLazy in-flight irrevocability:
   */
  bool
  TMLLazy::irrevoc(STM_IRREVOC_SIG(tx,upper_stack_bound))
  {
      // we are running in isolation by the time this code is run.  Make sure
      // we are valid.
      if (!bcasptr(&timestamp.val, tx->start_time, tx->start_time + 1))
          return false;

      // push all writes back to memory and clear writeset
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
      timestamp.val++;

      // return the STM to a state where it can be used after we finish our
      // irrevoc transaction
      tx->writes.reset();
      return true;
  }

  /**
   *  Switch to TMLLazy:
   *
   *    We just need to be sure that the timestamp is not odd
   */
  void
  TMLLazy::onSwitchTo()
  {
      if (timestamp.val & 1)
          ++timestamp.val;
  }
}

namespace stm {
  /**
   *  TMLLazy initialization
   */
  template<>
  void initTM<TMLLazy>()
  {
      // set the name
      stm::stms[TMLLazy].name     = "TMLLazy";

      // set the pointers
      stm::stms[TMLLazy].begin    = ::TMLLazy::begin;
      stm::stms[TMLLazy].commit   = ::TMLLazy::commit_ro;
      stm::stms[TMLLazy].read     = ::TMLLazy::read_ro;
      stm::stms[TMLLazy].write    = ::TMLLazy::write_ro;
      stm::stms[TMLLazy].rollback = ::TMLLazy::rollback;
      stm::stms[TMLLazy].irrevoc  = ::TMLLazy::irrevoc;
      stm::stms[TMLLazy].switcher = ::TMLLazy::onSwitchTo;
      stm::stms[TMLLazy].privatization_safe = true;
  }
}
