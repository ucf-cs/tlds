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
 *  LLT Implementation
 *
 *    This STM very closely resembles the GV1 variant of TL2.  That is, it uses
 *    orecs and lazy acquire.  Its clock requires everyone to increment it to
 *    commit writes, but this allows for read-set validation to be skipped at
 *    commit time.  Most importantly, there is no in-flight validation: if a
 *    timestamp is greater than when the transaction sampled the clock at begin
 *    time, the transaction aborts.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::TxThread;
using stm::timestamp;
using stm::timestamp_max;
using stm::WriteSet;
using stm::OrecList;
using stm::UNRECOVERABLE;
using stm::WriteSetEntry;
using stm::orec_t;
using stm::get_orec;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct LLT
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
      static NOINLINE void validate(TxThread*);
  };

  /**
   *  LLT begin:
   */
  bool
  LLT::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      // get a start time
      tx->start_time = timestamp.val;
      return false;
  }

  /**
   *  LLT commit (read-only):
   */
  void
  LLT::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // read-only, so just reset lists
      tx->r_orecs.reset();
      OnReadOnlyCommit(tx);
  }

  /**
   *  LLT commit (writing context):
   *
   *    Get all locks, validate, do writeback.  Use the counter to avoid some
   *    validations.
   */
  void
  LLT::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // acquire locks
      foreach (WriteSet, i, tx->writes) {
          // get orec, read its version#
          orec_t* o = get_orec(i->addr);
          uintptr_t ivt = o->v.all;

          // lock all orecs, unless already locked
          if (ivt <= tx->start_time) {
              // abort if cannot acquire
              if (!bcasptr(&o->v.all, ivt, tx->my_lock.all))
                  tx->tmabort(tx);
              // save old version to o->p, remember that we hold the lock
              o->p = ivt;
              tx->locks.insert(o);
          }
          // else if we don't hold the lock abort
          else if (ivt != tx->my_lock.all) {
              tx->tmabort(tx);
          }
      }

      // increment the global timestamp since we have writes
      uintptr_t end_time = 1 + faiptr(&timestamp.val);

      // skip validation if nobody else committed
      if (end_time != (tx->start_time + 1))
          validate(tx);

      // run the redo log
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));

      // release locks
      CFENCE;
      foreach (OrecList, i, tx->locks)
          (*i)->v.all = end_time;

      // clean-up
      tx->r_orecs.reset();
      tx->writes.reset();
      tx->locks.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  LLT read (read-only transaction)
   *
   *    We use "check twice" timestamps in LLT
   */
  void*
  LLT::read_ro(STM_READ_SIG(tx,addr,))
  {
      // get the orec addr
      orec_t* o = get_orec(addr);

      // read orec, then val, then orec
      uintptr_t ivt = o->v.all;
      CFENCE;
      void* tmp = *addr;
      CFENCE;
      uintptr_t ivt2 = o->v.all;
      // if orec never changed, and isn't too new, the read is valid
      if ((ivt <= tx->start_time) && (ivt == ivt2)) {
          // log orec, return the value
          tx->r_orecs.insert(o);
          return tmp;
      }
      // unreachable
      tx->tmabort(tx);
      return NULL;
  }

  /**
   *  LLT read (writing transaction)
   */
  void*
  LLT::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      // check the log for a RAW hazard, we expect to miss
      WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));
      bool found = tx->writes.find(log);
      REDO_RAW_CHECK(found, log, mask);

      // get the orec addr
      orec_t* o = get_orec(addr);

      // read orec, then val, then orec
      uintptr_t ivt = o->v.all;
      CFENCE;
      void* tmp = *addr;
      CFENCE;
      uintptr_t ivt2 = o->v.all;

      // fixup is here to minimize the postvalidation orec read latency
      REDO_RAW_CLEANUP(tmp, found, log, mask);
      // if orec never changed, and isn't too new, the read is valid
      if ((ivt <= tx->start_time) && (ivt == ivt2)) {
          // log orec, return the value
          tx->r_orecs.insert(o);
          return tmp;
      }
      tx->tmabort(tx);
      // unreachable
      return NULL;
  }

  /**
   *  LLT write (read-only context)
   */
  void
  LLT::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // add to redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  LLT write (writing context)
   */
  void
  LLT::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // add to redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
  }

  /**
   *  LLT unwinder:
   */
  stm::scope_t*
  LLT::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      // release the locks and restore version numbers
      foreach (OrecList, i, tx->locks)
          (*i)->v.all = (*i)->p;

      // undo memory operations, reset lists
      tx->r_orecs.reset();
      tx->writes.reset();
      tx->locks.reset();
      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  LLT in-flight irrevocability:
   */
  bool
  LLT::irrevoc(STM_IRREVOC_SIG(,))
  {
      return false;
  }

  /**
   *  LLT validation
   */
  void
  LLT::validate(TxThread* tx)
  {
      // validate
      foreach (OrecList, i, tx->r_orecs) {
          uintptr_t ivt = (*i)->v.all;
          // if unlocked and newer than start time, abort
          if ((ivt > tx->start_time) && (ivt != tx->my_lock.all))
              tx->tmabort(tx);
      }
  }

  /**
   *  Switch to LLT:
   *
   *    The timestamp must be >= the maximum value of any orec.  Some algs use
   *    timestamp as a zero-one mutex.  If they do, then they back up the
   *    timestamp first, in timestamp_max.
   */
  void
  LLT::onSwitchTo()
  {
      timestamp.val = MAXIMUM(timestamp.val, timestamp_max.val);
  }
}

namespace stm {
  /**
   *  LLT initialization
   */
  template<>
  void initTM<LLT>()
  {
      // set the name
      stms[LLT].name      = "LLT";

      // set the pointers
      stms[LLT].begin     = ::LLT::begin;
      stms[LLT].commit    = ::LLT::commit_ro;
      stms[LLT].read      = ::LLT::read_ro;
      stms[LLT].write     = ::LLT::write_ro;
      stms[LLT].rollback  = ::LLT::rollback;
      stms[LLT].irrevoc   = ::LLT::irrevoc;
      stms[LLT].switcher  = ::LLT::onSwitchTo;
      stms[LLT].privatization_safe = false;
  }
}
