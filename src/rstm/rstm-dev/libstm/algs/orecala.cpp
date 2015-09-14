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
 *  OrecALA Implementation
 *
 *    This is similar to the Detlefs algorithm for privatization-safe STM,
 *    TL2-IP, and [Marathe et al. ICPP 2008].  We use commit time ordering to
 *    ensure that there are no delayed cleanup problems, and we poll the
 *    timestamp variable to address doomed transactions.  By using TL2-style
 *    timestamps, we also achieve ALA publication safety
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::TxThread;
using stm::timestamp;
using stm::timestamp_max;
using stm::last_complete;
using stm::WriteSet;
using stm::OrecList;
using stm::orec_t;
using stm::get_orec;
using stm::WriteSetEntry;
using stm::UNRECOVERABLE;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct OrecALA {
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
      static NOINLINE void privtest(TxThread* tx, uintptr_t ts);
  };

  /**
   *  OrecALA begin:
   *
   *    We need a starting point for the transaction.  If an in-flight
   *    transaction is committed, but still doing writeback, we can either start
   *    at the point where that transaction had not yet committed, or else we can
   *    wait for it to finish writeback.  In this code, we choose the former
   *    option.
   *
   *    NB: the latter option might be better, since there is no timestamp
   *        scaling
   */
  bool
  OrecALA::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      // Start after the last cleanup, instead of after the last commit, to
      // avoid spinning in begin()
      tx->start_time = last_complete.val;
      tx->ts_cache = tx->start_time;
      tx->end_time = 0;
      return false;
  }

  /**
   *  OrecALA commit (read-only):
   *
   *    RO commit is trivial
   */
  void
  OrecALA::commit_ro(STM_COMMIT_SIG(tx,))
  {
      tx->r_orecs.reset();
      OnReadOnlyCommit(tx);
  }

  /**
   *  OrecALA commit (writing context):
   *
   *    OrecALA commit is like LLT: we get the locks, increment the counter, and
   *    then validate and do writeback.  As in other systems, some increments
   *    lead to skipping validation.
   *
   *    After writeback, we use a second, trailing counter to know when all txns
   *    who incremented the counter before this tx are done with writeback.  Only
   *    then can this txn mark its writeback complete.
   */
  void
  OrecALA::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // acquire locks
      foreach (WriteSet, i, tx->writes) {
          // get orec, read its version#
          orec_t* o = get_orec(i->addr);
          uintptr_t ivt = o->v.all;

          // if orec not locked, lock it and save old to orec.p
          if (ivt <= tx->start_time) {
              // abort if cannot acquire
              if (!bcasptr(&o->v.all, ivt, tx->my_lock.all))
                  tx->tmabort(tx);
              // save old version to o->p, remember that we hold the lock
              o->p = ivt;
              tx->locks.insert(o);
          }
          else if (ivt != tx->my_lock.all) {
              tx->tmabort(tx);
          }
      }

      // increment the global timestamp
      tx->end_time = 1 + faiptr(&timestamp.val);

      // skip validation if nobody committed since my last validation
      if (tx->end_time != (tx->ts_cache + 1)) {
          foreach (OrecList, i, tx->r_orecs) {
              // read this orec
              uintptr_t ivt = (*i)->v.all;
              if ((ivt > tx->start_time) && (ivt != tx->my_lock.all))
                  tx->tmabort(tx);
          }
      }

      // run the redo log
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));

      // release locks
      CFENCE;
      foreach (OrecList, i, tx->locks)
          (*i)->v.all = tx->end_time;

      // now ensure that transactions depart from stm_end in the order that
      // they incremend the timestamp.  This avoids the "deferred update"
      // half of the privatization problem.
      while (last_complete.val != (tx->end_time - 1))
          spin64();
      last_complete.val = tx->end_time;

      // clean-up
      tx->r_orecs.reset();
      tx->writes.reset();
      tx->locks.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  OrecALA read (read-only transaction)
   *
   *    Standard tl2-style read, but then we poll for potential privatization
   *    conflicts
   */
  void*
  OrecALA::read_ro(STM_READ_SIG(tx,addr,))
  {
      // read the location, log the orec
      void* tmp = *addr;
      orec_t* o = get_orec(addr);
      tx->r_orecs.insert(o);
      CFENCE;

      // make sure this location isn't locked or too new
      if (o->v.all > tx->start_time)
          tx->tmabort(tx);

      // privatization safety: poll the timestamp, maybe validate
      uintptr_t ts = timestamp.val;
      if (ts != tx->ts_cache)
          privtest(tx, ts);
      // return the value we read
      return tmp;
  }

  /**
   *  OrecALA read (writing transaction)
   *
   *    Same as above, but with a writeset lookup.
   */
  void*
  OrecALA::read_rw(STM_READ_SIG(tx,addr,mask))
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
   *  OrecALA write (read-only context)
   *
   *    Buffer the write, and switch to a writing context.
   */
  void
  OrecALA::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  OrecALA write (writing context)
   *
   *    Buffer the write
   */
  void
  OrecALA::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
  }

  /**
   *  OrecALA rollback:
   *
   *    This is a standard orec unwind function.  The only catch is that if a
   *    transaction aborted after incrementing the timestamp, it must wait its
   *    turn and then increment the trailing timestamp, to keep the two counters
   *    consistent.
   */
  stm::scope_t*
  OrecALA::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      // release the locks and restore version numbers
      foreach (OrecList, i, tx->locks)
          (*i)->v.all = (*i)->p;
      tx->r_orecs.reset();
      tx->writes.reset();
      tx->locks.reset();

      // if we aborted after incrementing the timestamp, then we have to
      // participate in the global cleanup order to support our solution to
      // the deferred update half of the privatization problem.
      // NB:  Note that end_time is always zero for restarts and retrys
      if (tx->end_time != 0) {
          while (last_complete.val < (tx->end_time - 1))
              spin64();
          last_complete.val = tx->end_time;
      }
      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  OrecALA in-flight irrevocability:
   *
   *    Either commit the transaction or return false.  Note that we're already
   *    serial by the time this code runs.
   */
  bool
  OrecALA::irrevoc(STM_IRREVOC_SIG(,))
  {
      return false;
  }

  /**
   *  OrecALA validation
   *
   *    an in-flight transaction must make sure it isn't suffering from the
   *    "doomed transaction" half of the privatization problem.  We can get that
   *    effect by calling this after every transactional read.
   */
  void OrecALA::privtest(TxThread* tx, uintptr_t ts)
  {
      // optimized validation since we don't hold any locks
      foreach (OrecList, i, tx->r_orecs) {
          // if orec unlocked and newer than start time, it changed, so abort.
          // if locked, it's not locked by me so abort
          if ((*i)->v.all > tx->start_time)
              tx->tmabort(tx);
      }

      // remember that we validated at this time
      tx->ts_cache = ts;
  }

  /**
   *  Switch to OrecALA:
   *
   *    The timestamp must be >= the maximum value of any orec.  Some algs use
   *    timestamp as a zero-one mutex.  If they do, then they back up the
   *    timestamp first, in timestamp_max.
   *
   *    Also, last_complete must equal timestamp
   */
  void OrecALA::onSwitchTo()
  {
      timestamp.val = MAXIMUM(timestamp.val, timestamp_max.val);
      last_complete.val = timestamp.val;
  }
}

namespace stm {
  /**
   *  OrecALA initialization
   */
  template<>
  void initTM<OrecALA>()
  {
      // set the name
      stm::stms[OrecALA].name     = "OrecALA";

      // set the pointers
      stm::stms[OrecALA].begin    = ::OrecALA::begin;
      stm::stms[OrecALA].commit   = ::OrecALA::commit_ro;
      stm::stms[OrecALA].read     = ::OrecALA::read_ro;
      stm::stms[OrecALA].write    = ::OrecALA::write_ro;
      stm::stms[OrecALA].rollback = ::OrecALA::rollback;
      stm::stms[OrecALA].irrevoc  = ::OrecALA::irrevoc;
      stm::stms[OrecALA].switcher = ::OrecALA::onSwitchTo;
      stm::stms[OrecALA].privatization_safe = true;
  }
}
