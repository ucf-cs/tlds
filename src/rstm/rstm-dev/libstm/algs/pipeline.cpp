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
 *  Pipeline Implementation
 *
 *    This algorithm is inspired by FastPath [LCPC 2009], and by Oancea et
 *    al. SPAA 2009.  We induce a total order on transactions at start time,
 *    via a global counter, and then we require them to commit in this order.
 *    For concurrency control, we use an orec table, but atomics are not
 *    needed, since the counter also serves as a commit token.
 *
 *    In addition, the lead thread uses in-place writes, via a special
 *    version of the read and write functions.  However, the lead thread
 *    can't self-abort.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"
#include <stm/UndoLog.hpp> // STM_DO_MASKED_WRITE

using stm::TxThread;
using stm::threads;
using stm::threadcount;
using stm::last_complete;
using stm::timestamp;
using stm::timestamp_max;
using stm::orec_t;
using stm::get_orec;
using stm::OrecList;
using stm::WriteSet;
using stm::UNRECOVERABLE;
using stm::WriteSetEntry;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct Pipeline {
      static TM_FASTCALL bool begin(TxThread*);
      static TM_FASTCALL void* read_ro(STM_READ_SIG(,,));
      static TM_FASTCALL void* read_rw(STM_READ_SIG(,,));
      static TM_FASTCALL void* read_turbo(STM_READ_SIG(,,));
      static TM_FASTCALL void write_ro(STM_WRITE_SIG(,,,));
      static TM_FASTCALL void write_rw(STM_WRITE_SIG(,,,));
      static TM_FASTCALL void write_turbo(STM_WRITE_SIG(,,,));
      static TM_FASTCALL void commit_ro(STM_COMMIT_SIG(,));
      static TM_FASTCALL void commit_rw(STM_COMMIT_SIG(,));
      static TM_FASTCALL void commit_turbo(STM_COMMIT_SIG(,));


      static stm::scope_t* rollback(STM_ROLLBACK_SIG(,,,));
      static bool irrevoc(STM_IRREVOC_SIG(,));
      static void onSwitchTo();
      static NOINLINE void validate(TxThread*, uintptr_t finish_cache);
  };

  /**
   *  Pipeline begin:
   *
   *    Pipeline is very fair: on abort, we keep our old order.  Thus only if we
   *    are starting a new transaction do we get an order.  We always check if we
   *    are oldest, in which case we can move straight to turbo mode.
   *
   *    ts_cache is important: when this tx starts, it knows its commit time.
   *    However, earlier txns have not yet committed.  The difference between
   *    ts_cache and order tells how many transactions need to commit.  Whenever
   *    one does, this tx will need to validate.
   */
  bool
  Pipeline::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();

      // only get a new start time if we didn't just abort
      if (tx->order == -1)
          tx->order = 1 + faiptr(&timestamp.val);

      tx->ts_cache = last_complete.val;
      if (tx->ts_cache == ((uintptr_t)tx->order - 1))
          GoTurbo(tx, read_turbo, write_turbo, commit_turbo);
      return false;
  }

  /**
   *  Pipeline commit (read-only):
   *
   *    For the sake of ordering, read-only transactions must wait until they
   *    are the oldest, then they validate.  This introduces a lot of
   *    overhead, but it gives SGLA (in the [Menon SPAA 2008] sense)
   *    semantics.
   */
  void
  Pipeline::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // wait our turn, then validate
      while (last_complete.val != ((uintptr_t)tx->order - 1)) {
          // in this wait loop, we need to check if an adaptivity action is
          // underway :(
          if (TxThread::tmbegin != begin)
              tx->tmabort(tx);
      }
      foreach (OrecList, i, tx->r_orecs) {
          // read this orec
          uintptr_t ivt = (*i)->v.all;
          // if it has a timestamp of ts_cache or greater, abort
          if (ivt > tx->ts_cache)
              tx->tmabort(tx);
      }
      // mark self as complete
      last_complete.val = tx->order;

      // set status to committed...
      tx->order = -1;

      // commit all frees, reset all lists
      tx->r_orecs.reset();
      OnReadOnlyCommit(tx);
  }

  /**
   *  Pipeline commit (writing context):
   *
   *    Given the total order, RW commit is just like RO commit, except that we
   *    need to acquire locks and do writeback, too.  One nice thing is that
   *    acquisition is with naked stores, and it is on a path that always
   *    commits.
   */
  void
  Pipeline::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // wait our turn, validate, writeback
      while (last_complete.val != ((uintptr_t)tx->order - 1)) {
          if (TxThread::tmbegin != begin)
              tx->tmabort(tx);
      }
      foreach (OrecList, i, tx->r_orecs) {
          // read this orec
          uintptr_t ivt = (*i)->v.all;
          // if it has a timestamp of ts_cache or greater, abort
          if (ivt > tx->ts_cache)
              tx->tmabort(tx);
      }
      // mark every location in the write set, and perform write-back
      // NB: we cannot abort anymore
      foreach (WriteSet, i, tx->writes) {
#ifdef STM_PROTECT_STACK
          volatile void* top_of_stack;
          if (i->addr >= &top_of_stack && i->addr < upper_stack_bound)
              continue;
#endif
          // get orec
          orec_t* o = get_orec(i->addr);
          // mark orec
          o->v.all = tx->order;
          CFENCE; // WBW
          // write-back
          *i->addr = i->val;
      }
      last_complete.val = tx->order;

      // set status to committed...
      tx->order = -1;

      // commit all frees, reset all lists
      tx->r_orecs.reset();
      tx->writes.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  Pipeline commit (turbo mode):
   *
   *    The current transaction is oldest, used in-place writes, and eagerly
   *    acquired all locks.  There is nothing to do but mark self as done.
   *
   *    NB: we do not distinguish between RO and RW... we should, and could
   *        via tx->writes
   */
  void
  Pipeline::commit_turbo(STM_COMMIT_SIG(tx,))
  {
      CFENCE;
      last_complete.val = tx->order;

      // set status to committed...
      tx->order = -1;

      // commit all frees, reset all lists
      tx->r_orecs.reset();
      tx->writes.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  Pipeline read (read-only transaction)
   *
   *    Since the commit time is determined before final validation (because the
   *    commit time is determined at begin time!), we can skip pre-validation.
   *    Otherwise, this is a standard orec read function.
   */
  void*
  Pipeline::read_ro(STM_READ_SIG(tx,addr,))
  {
      void* tmp = *addr;
      CFENCE; // RBR between dereference and orec check

      // get the orec addr, read the orec's version#
      orec_t* o = get_orec(addr);
      uintptr_t ivt = o->v.all;
      // abort if this changed since the last time I saw someone finish
      if (ivt > tx->ts_cache)
          tx->tmabort(tx);
      // log orec
      tx->r_orecs.insert(o);
      // validate if necessary
      if (last_complete.val > tx->ts_cache)
          validate(tx, last_complete.val);
      return tmp;
  }

  /**
   *  Pipeline read (writing transaction)
   */
  void*
  Pipeline::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      // check the log for a RAW hazard, we expect to miss
      WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));
      bool found = tx->writes.find(log);
      REDO_RAW_CHECK(found, log, mask);

      void* tmp = *addr;
      CFENCE; // RBR between dereference and orec check

      // get the orec addr, read the orec's version#
      orec_t* o = get_orec(addr);
      uintptr_t ivt = o->v.all;
      // abort if this changed since the last time I saw someone finish
      if (ivt > tx->ts_cache)
          tx->tmabort(tx);
      // log orec
      tx->r_orecs.insert(o);
      // validate if necessary
      if (last_complete.val > tx->ts_cache)
          validate(tx, last_complete.val);

      REDO_RAW_CLEANUP(tmp, found, log, mask)
      return tmp;
  }

  /**
   *  Pipeline read (turbo mode)
   */
  void*
  Pipeline::read_turbo(STM_READ_SIG(,addr,))
  {
      return *addr;
  }

  /**
   *  Pipeline write (read-only context)
   */
  void
  Pipeline::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // record the new value in a redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  Pipeline write (writing context)
   */
  void
  Pipeline::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // record the new value in a redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
  }

  /**
   *  Pipeline write (turbo mode)
   *
   *    The oldest transaction needs to mark the orec before writing in-place.
   */
  void
  Pipeline::write_turbo(STM_WRITE_SIG(tx,addr,val,mask))
  {
      orec_t* o = get_orec(addr);
      o->v.all = tx->order;
      CFENCE;
      STM_DO_MASKED_WRITE(addr, val, mask);
  }

  /**
   *  Pipeline unwinder:
   *
   *    For now, unwinding always happens before locks are held, and can't
   *    happen in turbo mode.
   *
   *    NB: Self-abort is not supported in Pipeline.  Adding undo logging to
   *        turbo mode would resolve the issue.
   */
  stm::scope_t*
  Pipeline::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);
      // we cannot be in fast mode
      if (CheckTurboMode(tx, read_turbo))
          UNRECOVERABLE("Attempting to abort a turbo-mode transaction!");

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      tx->r_orecs.reset();
      tx->writes.reset();
      // NB: at one time, this implementation could not reset pointers on
      //     abort.  This situation may remain, but it is not certain that it
      //     has not been resolved.
      return PostRollback(tx);
  }

  /**
   *  Pipeline in-flight irrevocability:
   */
  bool Pipeline::irrevoc(STM_IRREVOC_SIG(,))
  {
      UNRECOVERABLE("Pipeline Irrevocability not yet supported");
      return false;
  }

  /**
   *  Pipeline validation
   *
   *    Make sure all orec version#s are valid.  Then see about switching to
   *    turbo mode.  Note that to do the switch, the current write set must be
   *    written to memory.
   */
  void
  Pipeline::validate(TxThread* tx, uintptr_t finish_cache)
  {
      foreach (OrecList, i, tx->r_orecs) {
          // read this orec
          uintptr_t ivt = (*i)->v.all;
          // if it has a timestamp of ts_cache or greater, abort
          if (ivt > tx->ts_cache)
              tx->tmabort(tx);
      }
      // now update the finish_cache to remember that at this time, we were
      // still valid
      tx->ts_cache = finish_cache;
      // and if we are now the oldest thread, transition to fast mode
      if (tx->ts_cache == ((uintptr_t)tx->order - 1)) {
          if (tx->writes.size() != 0) {
              // mark every location in the write set, and perform write-back
              foreach (WriteSet, i, tx->writes) {
                  // get orec
                  orec_t* o = get_orec(i->addr);
                  // mark orec
                  o->v.all = tx->order;
                  CFENCE; // WBW
                  // write-back
                  *i->addr = i->val;
              }
              GoTurbo(tx, read_turbo, write_turbo, commit_turbo);
          }
      }
  }

  /**
   *  Switch to Pipeline:
   *
   *    The timestamp must be >= the maximum value of any orec.  Some algs use
   *    timestamp as a zero-one mutex.  If they do, then they back up the
   *    timestamp first, in timestamp_max.
   *
   *    Also, last_complete must equal timestamp
   *
   *    Also, all threads' order values must be -1
   */
  void
  Pipeline::onSwitchTo()
  {
      timestamp.val = MAXIMUM(timestamp.val, timestamp_max.val);
      last_complete.val = timestamp.val;
      for (uint32_t i = 0; i < threadcount.val; ++i)
          threads[i]->order = -1;
  }
}

namespace stm {
  /**
   *  Pipeline initialization
   */
  template<>
  void initTM<Pipeline>()
  {
      // set the name
      stms[Pipeline].name      = "Pipeline";

      // set the pointers
      stms[Pipeline].begin     = ::Pipeline::begin;
      stms[Pipeline].commit    = ::Pipeline::commit_ro;
      stms[Pipeline].read      = ::Pipeline::read_ro;
      stms[Pipeline].write     = ::Pipeline::write_ro;
      stms[Pipeline].rollback  = ::Pipeline::rollback;
      stms[Pipeline].irrevoc   = ::Pipeline::irrevoc;
      stms[Pipeline].switcher  = ::Pipeline::onSwitchTo;
      stms[Pipeline].privatization_safe = true;
  }
}
