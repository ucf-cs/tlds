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
 *  BitEager Implementation
 *
 *    This STM resembles TLRW, except that it uses an RSTM-style visible reader
 *    bitmap instead of TLRW-style bytelocks.  Like TLRW, we use timeout rather
 *    than remote abort.
 */

#include "../profiling.hpp"
#include "algs.hpp"

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::BitLockList;
using stm::bitlock_t;
using stm::get_bitlock;
using stm::rrec_t;
using stm::UndoLogEntry;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct BitEager
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
   *  Backoff parameters
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
   *  BitEager begin:
   */
  bool
  BitEager::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      return false;
  }

  /**
   *  BitEager commit (read-only):
   */
  void
  BitEager::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // read-only... release read locks
      foreach (BitLockList, i, tx->r_bitlocks)
          (*i)->readers.unsetbit(tx->id-1);

      tx->r_bitlocks.reset();
      OnReadOnlyCommit(tx);
  }

  /**
   *  BitEager commit (writing context):
   */
  void
  BitEager::commit_rw(STM_COMMIT_SIG(tx,))
  {
      // release write locks, then read locks
      foreach (BitLockList, i, tx->w_bitlocks)
          (*i)->owner = 0;
      foreach (BitLockList, i, tx->r_bitlocks)
          (*i)->readers.unsetbit(tx->id-1);

      // clean-up
      tx->r_bitlocks.reset();
      tx->w_bitlocks.reset();
      tx->undo_log.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  BitEager read (read-only transaction)
   *
   *    This is a timeout-based pessimistic algorithm: try to get a read lock
   *    (there must not be a writer, and WBR issues apply), then read directly
   *    from memory.
   */
  void*
  BitEager::read_ro(STM_READ_SIG(tx,addr,))
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
          while (lock->owner != 0)
              if (++tries > READ_TIMEOUT)
                  tx->tmabort(tx);
      }
  }

  /**
   *  BitEager read (writing transaction)
   *
   *    This is almost identical to the RO case, except that if the caller has
   *    the write lock, we can return immediately.
   */
  void*
  BitEager::read_rw(STM_READ_SIG(tx,addr,))
  {
      uint32_t tries = 0;
      bitlock_t* lock = get_bitlock(addr);

      // do I have the write lock?
      if (lock->owner == tx->id)
          return *addr;

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
          while (lock->owner != 0)
              if (++tries > READ_TIMEOUT)
                  tx->tmabort(tx);
      }
  }

  /**
   *  BitEager write (read-only context)
   *
   *    To write, we acquire the lock via CAS, then wait for all readers to drain
   *    out.
   */
  void
  BitEager::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
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

      // add to undo log, do in-place write
      tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
      STM_DO_MASKED_WRITE(addr, val, mask);

      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  BitEager write (writing context)
   *
   *    This is like the read-only case, except we might already hold the lock.
   */
  void
  BitEager::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      uint32_t tries = 0;
      bitlock_t* lock = get_bitlock(addr);

      // If I have the write lock, add to undo log, do write, return
      if (lock->owner == tx->id) {
          tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
          STM_DO_MASKED_WRITE(addr, val, mask);
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
      //
      // NB: we're spinning on 32 threads at a time... that might necessitate
      //     re-tuning the backoff parameters, but it's very efficient.
      for (unsigned b = 0; b < rrec_t::BUCKETS; ++b) {
          tries = 0;
          while (lock->readers.bits[b])
              if (++tries > DRAIN_TIMEOUT)
                  tx->tmabort(tx);
      }

      // add to undo log, do in-place write
      tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
      STM_DO_MASKED_WRITE(addr, val, mask);
  }

  /**
   *  BitEager unwinder:
   */
  stm::scope_t*
  BitEager::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // undo all writes
      STM_UNDO(tx->undo_log, upper_stack_bound, except, len);

      // release write locks, then read locks
      foreach (BitLockList, i, tx->w_bitlocks)
          (*i)->owner = 0;
      foreach (BitLockList, i, tx->r_bitlocks)
          (*i)->readers.unsetbit(tx->id-1);

      // reset lists
      tx->r_bitlocks.reset();
      tx->w_bitlocks.reset();
      tx->undo_log.reset();

      // randomized exponential backoff
      exp_backoff(tx);

      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  BitEager in-flight irrevocability:
   */
  bool BitEager::irrevoc(STM_IRREVOC_SIG(,))
  {
      return false;
  }

  /**
   *  Switch to BitEager:
   *
   *    When switching to BitEager, we don't have to do anything special.  The
   *    only global metadata used by BitEager is the bitlocks array, which should
   *    be all zeros.
   */
  void BitEager::onSwitchTo() {
  }
}

namespace stm {

  /**
   *  BitEager initialization
   */
  template<>
  void initTM<BitEager>()
  {
      // set the name
      stms[BitEager].name      = "BitEager";

      // set the pointers
      stms[BitEager].begin     = ::BitEager::begin;
      stms[BitEager].commit    = ::BitEager::commit_ro;
      stms[BitEager].read      = ::BitEager::read_ro;
      stms[BitEager].write     = ::BitEager::write_ro;
      stms[BitEager].rollback  = ::BitEager::rollback;
      stms[BitEager].irrevoc   = ::BitEager::irrevoc;
      stms[BitEager].switcher  = ::BitEager::onSwitchTo;
      stms[BitEager].privatization_safe = true;
  }
}
