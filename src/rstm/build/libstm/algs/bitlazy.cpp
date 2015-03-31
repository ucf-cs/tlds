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
 *  BitLazy Implementation
 *
 *    This is an unpublished STM algorithm.
 *
 *    We use RSTM-style visible reader bitmaps (actually, FairSTM-style
 *    visreader bitmaps), with lazy acquire.  Unlike RSTM, this is a lock-based
 *    (blocking) STM.
 *
 *    During execution, the transaction marks all *reads and writes* as reads,
 *    and then at commit time, it accumulates all potential conflicts, aborts
 *    all conflicting threads, and then does write-back.
 *
 *    Performance is quite bad, due to the CAS on each load, and O(R) CASes
 *    after committing (to release read locks).  It would be interesting to see
 *    how eager acquire fared, if there are any optimizations to the code to
 *    make things less costly, and how TLRW variants compare to this code.
 *    'Atomic or' might be useful, too.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::TxThread;
using stm::BitLockList;
using stm::WriteSet;
using stm::UNRECOVERABLE;
using stm::rrec_t;
using stm::bitlock_t;
using stm::get_bitlock;
using stm::threads;
using stm::WriteSetEntry;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct BitLazy
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
   *  BitLazy begin:
   */
  bool
  BitLazy::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      tx->alive = 1;
      return false;
  }

  /**
   *  BitLazy commit (read-only):
   */
  void
  BitLazy::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // were there remote aborts?
      if (!tx->alive)
          tx->tmabort(tx);
      CFENCE;

      // release read locks
      foreach (BitLockList, i, tx->r_bitlocks)
          (*i)->readers.unsetbit(tx->id-1);

      tx->r_bitlocks.reset();
      OnReadOnlyCommit(tx);
  }

  /**
   *  BitLazy commit (writing context):
   *
   *    First, get a lock on every location in the write set.  While locking
   *    locations, the tx will accumulate a list of all transactions with which
   *    it conflicts.  Then the tx will force those transactions to abort.  If
   *    the transaction is still alive at that point, it will redo its writes,
   *    release locks, and clean up.
   */
  void
  BitLazy::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // try to lock every location in the write set
      rrec_t accumulator = {{0}};
      // acquire locks, accumulate victim readers
      foreach (WriteSet, i, tx->writes) {
          // get bitlock, read its version#
          bitlock_t* bl = get_bitlock(i->addr);
          // abort if cannot acquire and haven't locked yet
          if (bl->owner == 0) {
              if (!bcasptr(&bl->owner, (uintptr_t)0, tx->my_lock.all))
                  tx->tmabort(tx);
              // log lock
              tx->w_bitlocks.insert(bl);
              // get readers
              accumulator |= bl->readers;
          }
          else if (bl->owner != tx->my_lock.all) {
              tx->tmabort(tx);
          }
      }

      // take me out of the accumulator
      accumulator.bits[(tx->id-1)/(8*sizeof(uintptr_t))] &=
          ~(1lu << ((tx->id-1) % (8*sizeof(uintptr_t))));
      // kill conflicting readers
      for (unsigned b = 0; b < rrec_t::BUCKETS; ++b) {
          if (accumulator.bits[b]) {
              for (unsigned c = 0; c < (8*sizeof(uintptr_t)); c++) {
                  if (accumulator.bits[b] & (1ul << c)) {
                      // need atomic for x86 ordering... WBR insufficient
                      //
                      // NB: This CAS seems very expensive.  We could
                      //     probably use regular writes here, as long as we
                      //     enforce the ordering we need later on, e.g., via
                      //     a phony xchg
                      cas32(&threads[(8*sizeof(uintptr_t))*b+c]->alive,
                            1u, 0u);
                  }
              }
          }
      }

      // were there remote aborts?
      CFENCE;
      if (!tx->alive)
          tx->tmabort(tx);
      CFENCE;

      // we committed... replay redo log
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
      CFENCE;

      // release read locks, write locks
      foreach (BitLockList, i, tx->w_bitlocks)
          (*i)->owner = 0;
      foreach (BitLockList, i, tx->r_bitlocks)
          (*i)->readers.unsetbit(tx->id-1);

      // remember that this was a commit
      tx->r_bitlocks.reset();
      tx->writes.reset();
      tx->w_bitlocks.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  BitLazy read (read-only transaction)
   *
   *    Must preserve write-before-read ordering between marking self as a reader
   *    and checking for conflicting writers.
   */
  void*
  BitLazy::read_ro(STM_READ_SIG(tx,addr,))
  {
      // first test if we've got a read bit
      bitlock_t* bl = get_bitlock(addr);
      if (bl->readers.setif(tx->id-1))
          tx->r_bitlocks.insert(bl);
      // if there's a writer, it can't be me since I'm in-flight
      if (bl->owner)
          tx->tmabort(tx);
      // order the read before checking for remote aborts
      void* val = *addr;
      CFENCE;
      if (!tx->alive)
          tx->tmabort(tx);
      return val;
  }

  /**
   *  BitLazy read (writing transaction)
   *
   *    Same as above, but with a test if this tx has a pending write
   */
  void*
  BitLazy::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      // Used by REDO_RAW_CLEANUP so they have to be scoped out here. We assume
      // that the compiler will do a good job when byte-logging isn't enabled in
      // compiling this.
      bool found = false;
      WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));

      // first test if we've got a read bit
      bitlock_t* bl = get_bitlock(addr);
      if (bl->readers.setif(tx->id-1))
          tx->r_bitlocks.insert(bl);
      // if so, we may be a writer (all writes are also reads!)
      else {
          found = tx->writes.find(log);
          REDO_RAW_CHECK(found, log, mask);
      }
      if (bl->owner)
          tx->tmabort(tx);
      void* val = *addr;
      REDO_RAW_CLEANUP(val, found, log, mask);
      CFENCE;
      if (!tx->alive)
          tx->tmabort(tx);
      return val;
  }

  /**
   *  BitLazy write (read-only context)
   *
   *    Log the write, and then mark the location as if reading.
   */
  void
  BitLazy::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // Record the new value in a redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));

      // if we don't have a read bit, get one
      bitlock_t* bl = get_bitlock(addr);
      if (bl->readers.setif(tx->id-1))
          tx->r_bitlocks.insert(bl);
      if (bl->owner)
          tx->tmabort(tx);
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  BitLazy write (writing context)
   */
  void
  BitLazy::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // Record the new value in a redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));

      // if we don't have a read bit, get one
      bitlock_t* bl = get_bitlock(addr);
      if (bl->readers.setif(tx->id-1))
          tx->r_bitlocks.insert(bl);
      if (bl->owner)
          tx->tmabort(tx);
  }

  /**
   *  BitLazy unwinder:
   */
  stm::scope_t*
  BitLazy::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      // release the locks
      foreach (BitLockList, i, tx->w_bitlocks)
          (*i)->owner = 0;
      foreach (BitLockList, i, tx->r_bitlocks)
          (*i)->readers.unsetbit(tx->id-1);

      tx->r_bitlocks.reset();
      tx->writes.reset();
      tx->w_bitlocks.reset();

      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  BitLazy in-flight irrevocability:
   */
  bool BitLazy::irrevoc(STM_IRREVOC_SIG(,))
  {
      return false;
  }

  /**
   *  Switch to BitLazy:
   *
   *    The BitLock array should be all zeroes when we start using this algorithm
   */
  void BitLazy::onSwitchTo() {
  }
}

namespace stm {
  /**
   *  BitLazy initialization
   */
  template<>
  void initTM<BitLazy>()
  {
      // set the name
      stms[BitLazy].name      = "BitLazy";

      // set the pointers
      stms[BitLazy].begin     = ::BitLazy::begin;
      stms[BitLazy].commit    = ::BitLazy::commit_ro;
      stms[BitLazy].read      = ::BitLazy::read_ro;
      stms[BitLazy].write     = ::BitLazy::write_ro;
      stms[BitLazy].rollback  = ::BitLazy::rollback;
      stms[BitLazy].irrevoc   = ::BitLazy::irrevoc;
      stms[BitLazy].switcher  = ::BitLazy::onSwitchTo;
      stms[BitLazy].privatization_safe = true;
  }
}
