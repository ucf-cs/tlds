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
 *  BitEagerRedo Implementation
 *
 *    This is like BitEager, but instead of in-place update, we use redo logs.
 *    Note that we still have eager acquire.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::BitLockList;
using stm::bitlock_t;
using stm::get_bitlock;
using stm::WriteSetEntry;
using stm::rrec_t;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct BitEagerRedo
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
   *  These defines are for tuning backoff behavior
   */
#if defined(STM_CPU_SPARC)
#  define READ_TIMEOUT        32
#  define ACQUIRE_TIMEOUT     128
#  define DRAIN_TIMEOUT       1024
#else // STM_CPU_X86
#  define READ_TIMEOUT        32
#  define ACQUIRE_TIMEOUT     128
#  define DRAIN_TIMEOUT       256
#endif

  /**
   *  BitEagerRedo begin:
   */
  bool
  BitEagerRedo::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      return false;
  }

  /**
   *  BitEagerRedo commit (read-only):
   */
  void
  BitEagerRedo::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // read-only... release read locks
      foreach (BitLockList, i, tx->r_bitlocks)
          (*i)->readers.unsetbit(tx->id-1);

      tx->r_bitlocks.reset();
      OnReadOnlyCommit(tx);
  }

  /**
   *  BitEagerRedo commit (writing context):
   */
  void
  BitEagerRedo::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // replay redo log
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
      CFENCE;

      // release write locks, then read locks
      foreach (BitLockList, i, tx->w_bitlocks)
          (*i)->owner = 0;
      foreach (BitLockList, i, tx->r_bitlocks)
          (*i)->readers.unsetbit(tx->id-1);

      // clean-up
      tx->r_bitlocks.reset();
      tx->w_bitlocks.reset();
      tx->writes.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  BitEagerRedo read (read-only transaction)
   *
   *    As in BitEager, we use timeout for conflict resolution
   */
  void*
  BitEagerRedo::read_ro(STM_READ_SIG(tx,addr,))
  {
      uint32_t tries = 0;
      bitlock_t* lock = get_bitlock(addr);

      // do I have a read lock?
      if (lock->readers.getbit(tx->id-1))
          return *addr;

      // log this location
      tx->r_bitlocks.insert(lock);

      // now try to get a read lock
      while (true) {
          // mark my reader bit
          lock->readers.setbit(tx->id-1);

          // if nobody has the write lock, we're done
          if (__builtin_expect(lock->owner == 0, true))
              return *addr;

          // drop read lock, wait (with timeout) for lock release
          lock->readers.unsetbit(tx->id-1);
          while (lock->owner != 0) {
              if (++tries > READ_TIMEOUT)
                  tx->tmabort(tx);
          }
      }
  }

  /**
   *  BitEagerRedo read (writing transaction)
   *
   *    Same as RO case, but if we have the write lock, we can take a fast path
   */
  void*
  BitEagerRedo::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      uint32_t tries = 0;
      bitlock_t* lock = get_bitlock(addr);

      // do I have the write lock?
      if (lock->owner == tx->id) {
          // check the log
          WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));
          bool found = tx->writes.find(log);
          REDO_RAW_CHECK(found, log, mask);

          void* val = *addr;
          REDO_RAW_CLEANUP(val, found, log, mask);
          return val;
      }

      // do I have a read lock?
      if (lock->readers.getbit(tx->id-1))
          return *addr;

      // log this location
      tx->r_bitlocks.insert(lock);

      // now try to get a read lock
      while (true) {
          // mark my reader bit
          lock->readers.setbit(tx->id-1);

          // if nobody has the write lock, we're done
          if (__builtin_expect(lock->owner == 0, true))
              return *addr;

          // drop read lock, wait (with timeout) for lock release
          lock->readers.unsetbit(tx->id-1);
          while (lock->owner != 0) {
              if (++tries > READ_TIMEOUT)
                  tx->tmabort(tx);
          }
      }
  }

  /**
   *  BitEagerRedo write (read-only context)
   *
   *    Lock the location, then put the value in the write log
   */
  void
  BitEagerRedo::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      uint32_t tries = 0;
      bitlock_t* lock = get_bitlock(addr);

      // get the write lock, with timeout
      while (!bcasptr(&(lock->owner), 0u, tx->id))
          if (++tries > ACQUIRE_TIMEOUT)
              tx->tmabort(tx);

      // log the lock, drop any read locks I have
      tx->w_bitlocks.insert(lock);
      lock->readers.unsetbit(tx->id-1);

      // wait (with timeout) for readers to drain out
      // (read one bucket at a time)
      for (unsigned b = 0; b < rrec_t::BUCKETS; ++b) {
          tries = 0;
          while (lock->readers.bits[b])
              if (++tries > DRAIN_TIMEOUT)
                  tx->tmabort(tx);
      }

      // record in redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));

      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  BitEagerRedo write (writing context)
   *
   *    Same as RO case, but with fastpath for repeat writes to same location
   */
  void
  BitEagerRedo::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      uint32_t tries = 0;
      bitlock_t* lock = get_bitlock(addr);

      // If I have the write lock, record in redo log, return
      if (lock->owner == tx->id) {
          tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
          return;
      }

      // get the write lock, with timeout
      while (!bcasptr(&(lock->owner), 0u, tx->id))
          if (++tries > ACQUIRE_TIMEOUT)
              tx->tmabort(tx);

      // log the lock, drop any read locks I have
      tx->w_bitlocks.insert(lock);
      lock->readers.unsetbit(tx->id-1);

      // wait (with timeout) for readers to drain out
      // (read one bucket at a time)
      for (unsigned b = 0; b < rrec_t::BUCKETS; ++b) {
          tries = 0;
          while (lock->readers.bits[b])
              if (++tries > DRAIN_TIMEOUT)
                  tx->tmabort(tx);
      }

      // record in redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
  }

  /**
   *  BitEagerRedo unwinder:
   */
  stm::scope_t*
  BitEagerRedo::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      // release write locks, then read locks
      foreach (BitLockList, i, tx->w_bitlocks)
          (*i)->owner = 0;
      foreach (BitLockList, i, tx->r_bitlocks)
          (*i)->readers.unsetbit(tx->id-1);

      // reset lists
      tx->r_bitlocks.reset();
      tx->w_bitlocks.reset();
      tx->writes.reset();

      // randomized exponential backoff
      exp_backoff(tx);

      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  BitEagerRedo in-flight irrevocability:
   */
  bool BitEagerRedo::irrevoc(STM_IRREVOC_SIG(,))
  {
      return false;
  }

  /**
   *  Switch to BitEagerRedo:
   *
   *    The only global metadata used by BitEagerRedo is the bitlocks array,
   *    which should be all zeros.
   */
  void BitEagerRedo::onSwitchTo() {
  }
}

namespace stm {
  /**
   *  BitEagerRedo initialization
   */
  template<>
  void initTM<BitEagerRedo>()
  {
      // set the name
      stms[BitEagerRedo].name      = "BitEagerRedo";

      // set the pointers
      stms[BitEagerRedo].begin     = ::BitEagerRedo::begin;
      stms[BitEagerRedo].commit    = ::BitEagerRedo::commit_ro;
      stms[BitEagerRedo].read      = ::BitEagerRedo::read_ro;
      stms[BitEagerRedo].write     = ::BitEagerRedo::write_ro;
      stms[BitEagerRedo].rollback  = ::BitEagerRedo::rollback;
      stms[BitEagerRedo].irrevoc   = ::BitEagerRedo::irrevoc;
      stms[BitEagerRedo].switcher  = ::BitEagerRedo::onSwitchTo;
      stms[BitEagerRedo].privatization_safe = true;
  }
}
