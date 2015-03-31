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
 *  ByteLazy Implementation
 *
 *    This is an unpublished algorithm.  It is identical to BitLazy, except
 *    that it uses TLRW-style ByteLocks instead of BitLocks.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::WriteSet;
using stm::ByteLockList;
using stm::bytelock_t;
using stm::get_bytelock;
using stm::WriteSetEntry;
using stm::threads;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct ByteLazy
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
   *  ByteLazy begin:
   */
  bool
  ByteLazy::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      // mark self as alive
      tx->alive = 1;
      return false;
  }

  /**
   *  ByteLazy commit (read-only):
   */
  void
  ByteLazy::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // were there remote aborts?
      if (!tx->alive)
          tx->tmabort(tx);
      CFENCE;

      // release read locks
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      // clean up
      tx->r_bytelocks.reset();
      OnReadOnlyCommit(tx);
  }

  /**
   *  ByteLazy commit (writing context):
   *
   *    First, get a lock on every location in the write set.  While locking
   *    locations, the tx will accumulate a list of all transactions with which
   *    it conflicts.  Then the tx will force those transactions to abort.  If
   *    the transaction is still alive at that point, it will redo its writes,
   *    release locks, and clean up.
   */
  void
  ByteLazy::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // try to lock every location in the write set
      unsigned char accumulator[60] = {0};
      // acquire locks, accumulate victim readers
      foreach (WriteSet, i, tx->writes) {
          // get bytelock, read its version#
          bytelock_t* bl = get_bytelock(i->addr);

          // abort if cannot acquire and haven't locked yet
          if (bl->owner == 0) {
              if (!bcas32(&bl->owner, (uintptr_t)0, tx->my_lock.all))
                  tx->tmabort(tx);

              // log lock
              tx->w_bytelocks.insert(bl);

              // get readers
              // (read 4 bytelocks at a time)
              volatile uint32_t* p1 = (volatile uint32_t*)&accumulator[0];
              volatile uint32_t* p2 = (volatile uint32_t*)&bl->reader[0];
              for (int j = 0; j < 15; ++j)
                  p1[j] |= p2[j];
          }
          else if (bl->owner != tx->my_lock.all) {
              tx->tmabort(tx);
          }
      }

      // take me out of the accumulator
      accumulator[tx->id-1] = 0;

      // kill the readers
      for (unsigned char c = 0; c < 60; ++c)
          if (accumulator[c] == 1)
              cas32(&threads[c]->alive, 1u, 0u);

      // were there remote aborts?
      CFENCE;
      if (!tx->alive)
          tx->tmabort(tx);
      CFENCE;

      // we committed... replay redo log
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
      CFENCE;

      // release read locks, write locks
      foreach (ByteLockList, i, tx->w_bytelocks)
          (*i)->owner = 0;
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      // remember that this was a commit
      tx->r_bytelocks.reset();
      tx->writes.reset();
      tx->w_bytelocks.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  ByteLazy read (read-only transaction)
   */
  void*
  ByteLazy::read_ro(STM_READ_SIG(tx,addr,))
  {
      // first test if we've got a read byte
      bytelock_t* bl = get_bytelock(addr);

      // lock and log if the byte is previously unlocked
      if (bl->reader[tx->id-1] == 0) {
          bl->set_read_byte(tx->id-1);
          // log the lock
          tx->r_bytelocks.insert(bl);
      }

      // if there's a writer, it can't be me since I'm in-flight
      if (bl->owner != 0)
          tx->tmabort(tx);

      // order the read before checking for remote aborts
      void* val = *addr;
      CFENCE;

      if (!tx->alive)
          tx->tmabort(tx);

      return val;
  }

  /**
   *  ByteLazy read (writing transaction)
   */
  void*
  ByteLazy::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      // These are used in REDO_RAW_CLEANUP, so they have to be scoped out
      // here. We expect the compiler to do a good job reordering them when
      // this macro is empty (when word-logging).
      bool found = false;
      WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));

      // first test if we've got a read byte
      bytelock_t* bl = get_bytelock(addr);

      // lock and log if the byte is previously unlocked
      if (bl->reader[tx->id-1] == 0) {
          bl->set_read_byte(tx->id-1);
          // log the lock
          tx->r_bytelocks.insert(bl);
      } else {
          // if so, we may be a writer (all writes are also reads!)
          // check the log
          found = tx->writes.find(log);
          REDO_RAW_CHECK(found, log, mask);
      }

      // if there's a writer, it can't be me since I'm in-flight
      if (bl->owner != 0)
          tx->tmabort(tx);

      // order the read before checking for remote aborts
      void* val = *addr;
      REDO_RAW_CLEANUP(val, found, log, mask);
      CFENCE;

      if (!tx->alive)
          tx->tmabort(tx);

      return val;
  }

  /**
   *  ByteLazy write (read-only context)
   *
   *    In this implementation, every write is a read during execution, so mark
   *    this location as if it was a read.
   */
  void
  ByteLazy::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // Record the new value in a redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));

      // if we don't have a read byte, get one
      bytelock_t* bl = get_bytelock(addr);
      if (bl->reader[tx->id-1] == 0) {
          bl->set_read_byte(tx->id-1);
          // log the lock
          tx->r_bytelocks.insert(bl);
      }

      if (bl->owner)
          tx->tmabort(tx);

      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  ByteLazy write (writing context)
   */
  void
  ByteLazy::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // Record the new value in a redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));

      // if we don't have a read byte, get one
      bytelock_t* bl = get_bytelock(addr);
      if (bl->reader[tx->id-1] == 0) {
          bl->set_read_byte(tx->id-1);
          // log the lock
          tx->r_bytelocks.insert(bl);
      }

      if (bl->owner)
          tx->tmabort(tx);
  }

  /**
   *  ByteLazy unwinder:
   */
  stm::scope_t*
  ByteLazy::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      // release the locks
      foreach (ByteLockList, i, tx->w_bytelocks)
          (*i)->owner = 0;
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      // clear all lists
      tx->r_bytelocks.reset();
      tx->writes.reset();
      tx->w_bytelocks.reset();

      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  ByteLazy in-flight irrevocability:
   */
  bool
  ByteLazy::irrevoc(STM_IRREVOC_SIG(,))
  {
      return false;
  }

  /**
   *  Switch to ByteLazy:
   */
  void
  ByteLazy::onSwitchTo() {
  }
}

namespace stm {
  /**
   *  ByteLazy initialization
   */
  template<>
  void initTM<ByteLazy>()
  {
      // set the name
      stms[ByteLazy].name      = "ByteLazy";

      // set the pointers
      stms[ByteLazy].begin     = ::ByteLazy::begin;
      stms[ByteLazy].commit    = ::ByteLazy::commit_ro;
      stms[ByteLazy].read      = ::ByteLazy::read_ro;
      stms[ByteLazy].write     = ::ByteLazy::write_ro;
      stms[ByteLazy].rollback  = ::ByteLazy::rollback;
      stms[ByteLazy].irrevoc   = ::ByteLazy::irrevoc;
      stms[ByteLazy].switcher  = ::ByteLazy::onSwitchTo;
      stms[ByteLazy].privatization_safe = true;
  }
}
