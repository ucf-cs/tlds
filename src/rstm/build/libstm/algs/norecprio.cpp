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
 *  NOrecPrio Implementation
 *
 *    This is like NOrec, except that too many consecutive aborts result in
 *    this thread gaining priority.  When a thread has priority, lower-priority
 *    threads cannot commit if they are writers
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::TxThread;
using stm::timestamp;
using stm::KARMA_FACTOR;
using stm::prioTxCount;
using stm::threadcount;
using stm::threads;
using stm::WriteSetEntry;
using stm::ValueList;
using stm::ValueListEntry;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct NOrecPrio {
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

      static const uintptr_t VALIDATION_FAILED = 1;
      static NOINLINE uintptr_t validate(TxThread*);
  };

  /**
   *  NOrecPrio begin:
   *
   *    We're using the 'classic' NOrec begin technique here.  Also, we check if
   *    we need priority here, rather than retaining it across an abort.
   */
  bool
  NOrecPrio::begin(TxThread* tx)
  {
      // Sample the sequence lock until it is even (unheld)
      while ((tx->start_time = timestamp.val) & 1)
          spin64();

      // notify the allocator
      tx->allocator.onTxBegin();

      // handle priority
      long prio_bump = tx->consec_aborts / KARMA_FACTOR;
      if (prio_bump) {
          faiptr(&prioTxCount.val);
          tx->prio = prio_bump;
      }

      return false;
  }

  /**
   *  NOrecPrio commit (read-only):
   *
   *    Standard NOrec RO commit, except that if we have priority, we must
   *    release it.
   */
  void
  NOrecPrio::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // read-only fastpath
      tx->vlist.reset();
      // priority
      if (tx->prio) {
          faaptr(&prioTxCount.val, -1);
          tx->prio = 0;
      }
      OnReadOnlyCommit(tx);
  }

  /**
   *  NOrecPrio commit (writing context):
   *
   *    This priority technique is imprecise.  Someone could gain priority while
   *    this thread is trying to acquire the CAS.  That's OK, because we just aim
   *    to be "fair", without any guarantees.
   */
  void
  NOrecPrio::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // wait for all higher-priority transactions to complete
      //
      // NB: we assume there are priority transactions, because we wouldn't be
      //     using the STM otherwise.
      while (true) {
          bool good = true;
          for (uintptr_t i = 0; i < threadcount.val; ++i)
              good = good && (threads[i]->prio <= tx->prio);
          if (good)
              break;
      }

      // get the lock and validate (use RingSTM obstruction-free technique)
      while (!bcasptr(&timestamp.val, tx->start_time, tx->start_time + 1))
          if ((tx->start_time = validate(tx)) == VALIDATION_FAILED)
              tx->tmabort(tx);

      // redo writes
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));

      // release the sequence lock, then clean up
      CFENCE;
      timestamp.val = tx->start_time + 2;
      tx->vlist.reset();
      tx->writes.reset();
      // priority
      if (tx->prio) {
          faaptr(&prioTxCount.val, -1);
          tx->prio = 0;
      }
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  NOrecPrio read (read-only transaction)
   *
   *    This is a standard NOrec read
   */
  void*
  NOrecPrio::read_ro(STM_READ_SIG(tx,addr,mask))
  {
      // read the location to a temp
      void* tmp = *addr;
      CFENCE;

      while (tx->start_time != timestamp.val) {
          if ((tx->start_time = validate(tx)) == VALIDATION_FAILED)
              tx->tmabort(tx);
          tmp = *addr;
          CFENCE;
      }

      // log the read
      tx->vlist.insert(STM_VALUE_LIST_ENTRY(addr, tmp, mask));
      return tmp;
  }

  /**
   *  NOrecPrio read (writing transaction)
   *
   *    Standard NOrec read from writing context
   */
  void*
  NOrecPrio::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      // check the log for a RAW hazard, we expect to miss
      WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));
      bool found = tx->writes.find(log);
      REDO_RAW_CHECK(found, log, mask);

      // Use the code from the read-only read barrier. This is complicated by
      // the fact that, when we are byte logging, we may have successfully read
      // some bytes from the write log (if we read them all then we wouldn't
      // make it here). In this case, we need to log the mask for the rest of the
      // bytes that we "actually" need, which is computed as bytes in mask but
      // not in log.mask. This is only correct because we know that a failed
      // find also reset the log.mask to 0 (that's part of the find interface).
      void* val = read_ro(tx, addr STM_MASK(mask & ~log.mask));
      REDO_RAW_CLEANUP(val, found, log, mask);
      return val;
  }

  /**
   *  NOrecPrio write (read-only context)
   *
   *    log the write and switch to a writing context
   */
  void
  NOrecPrio::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // do a buffered write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  NOrecPrio write (writing context)
   *
   *    log the write
   */
  void
  NOrecPrio::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // do a buffered write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
  }

  /**
   *  NOrecPrio unwinder:
   *
   *    If we abort, be sure to release priority
   */
  stm::scope_t*
  NOrecPrio::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      tx->vlist.reset();
      tx->writes.reset();
      // if I had priority, release it
      if (tx->prio) {
          faaptr(&prioTxCount.val, -1);
          tx->prio = 0;
      }
      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  NOrecPrio in-flight irrevocability: Getting priority right is very
   *  hard, so we're just going to use abort-and-restart
   */
  bool
  NOrecPrio::irrevoc(STM_IRREVOC_SIG(tx,upper_stack_bound))
  {
      return false;
  }

  /**
   *  NOrecPrio validation
   *
   *    Make sure that during some time period where the seqlock is constant
   *    and odd, all values in the read log are still present in memory.
   */
  uintptr_t
  NOrecPrio::validate(TxThread* tx)
  {
      while (true) {
          // read the lock until it is even
          uintptr_t s = timestamp.val;
          if ((s & 1) == 1)
              continue;

          // check the read set
          CFENCE;
          // don't branch in the loop---consider it backoff if we fail
          // validation early
          bool valid = true;
          foreach (ValueList, i, tx->vlist)
              valid &= i->isValid();

          if (!valid)
              return VALIDATION_FAILED;

          // restart if timestamp changed during read set iteration
          CFENCE;
          if (timestamp.val == s)
              return s;
      }
  }

  /**
   *  Switch to NOrecPrio:
   *
   *    Must be sure the timestamp is not odd.
   */
  void
  NOrecPrio::onSwitchTo()
  {
      if (timestamp.val & 1)
          ++timestamp.val;
  }
}

namespace stm {
  /**
   *  NOrecPrio initialization
   */
  template<>
  void initTM<NOrecPrio>()
  {
      // set the name
      stm::stms[NOrecPrio].name      = "NOrecPrio";

      // set the pointers
      stm::stms[NOrecPrio].begin    = ::NOrecPrio::begin;
      stm::stms[NOrecPrio].commit   = ::NOrecPrio::commit_ro;
      stm::stms[NOrecPrio].read     = ::NOrecPrio::read_ro;
      stm::stms[NOrecPrio].write    = ::NOrecPrio::write_ro;
      stm::stms[NOrecPrio].rollback = ::NOrecPrio::rollback;
      stm::stms[NOrecPrio].irrevoc  = ::NOrecPrio::irrevoc;
      stm::stms[NOrecPrio].switcher = ::NOrecPrio::onSwitchTo;
      stm::stms[NOrecPrio].privatization_safe = true;
  }
}
