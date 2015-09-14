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
 *  ByteEagerRedo Implementation
 *
 *    This is like ByteEager, except we use redo logs instead of undo logs.  We
 *    still use eager locking.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::ByteLockList;
using stm::bytelock_t;
using stm::get_bytelock;
using stm::WriteSetEntry;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct ByteEagerRedo
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
   *  ByteEagerRedo begin:
   */
  bool
  ByteEagerRedo::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      return false;
  }

  /**
   *  ByteEagerRedo commit (read-only):
   */
  void
  ByteEagerRedo::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // read-only... release read locks
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      tx->r_bytelocks.reset();
      OnReadOnlyCommit(tx);
  }

  /**
   *  ByteEagerRedo commit (writing context):
   */
  void
  ByteEagerRedo::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // replay redo log
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
      CFENCE;

      // release write locks, then read locks
      foreach (ByteLockList, i, tx->w_bytelocks)
          (*i)->owner = 0;
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      // clean-up
      tx->r_bytelocks.reset();
      tx->w_bytelocks.reset();
      tx->writes.reset();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  ByteEagerRedo read (read-only transaction)
   */
  void*
  ByteEagerRedo::read_ro(STM_READ_SIG(tx,addr,))
  {
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(addr);

      // do I have a read lock?
      if (lock->reader[tx->id-1] == 1)
          return *addr;

      // log this location
      tx->r_bytelocks.insert(lock);

      // now try to get a read lock
      while (true) {
          // mark my reader byte
          lock->set_read_byte(tx->id-1);
          // if nobody has the write lock, we're done
          if (__builtin_expect(lock->owner == 0, true))
              return *addr;

          // drop read lock, wait (with timeout) for lock release
          lock->reader[tx->id-1] = 0;
          while (lock->owner != 0) {
              if (++tries > READ_TIMEOUT)
                  tx->tmabort(tx);
          }
      }
  }

  /**
   *  ByteEagerRedo read (writing transaction)
   */
  void*
  ByteEagerRedo::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(addr);

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
      if (lock->reader[tx->id-1] == 1)
          return *addr;

      // log this location
      tx->r_bytelocks.insert(lock);

      // now try to get a read lock
      while (true) {
          // mark my reader byte
          lock->set_read_byte(tx->id-1);
          // if nobody has the write lock, we're done
          if (__builtin_expect(lock->owner == 0, true))
              return *addr;

          // drop read lock, wait (with timeout) for lock release
          lock->reader[tx->id-1] = 0;
          while (lock->owner != 0) {
              if (++tries > READ_TIMEOUT)
                  tx->tmabort(tx);
          }
      }
  }

  /**
   *  ByteEagerRedo write (read-only context)
   */
  void
  ByteEagerRedo::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(addr);

      // get the write lock, with timeout
      while (!bcas32(&(lock->owner), 0u, tx->id))
          if (++tries > ACQUIRE_TIMEOUT)
              tx->tmabort(tx);

      // log the lock, drop any read locks I have
      tx->w_bytelocks.insert(lock);
      lock->reader[tx->id-1] = 0;

      // wait (with timeout) for readers to drain out
      // (read 4 bytelocks at a time)
      volatile uint32_t* lock_alias = (volatile uint32_t*)&lock->reader[0];
      for (int i = 0; i < 15; ++i) {
          tries = 0;
          while (lock_alias[i] != 0)
              if (++tries > DRAIN_TIMEOUT)
                  tx->tmabort(tx);
      }

      // record in redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));

      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  ByteEagerRedo write (writing context)
   */
  void
  ByteEagerRedo::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      uint32_t tries = 0;
      bytelock_t* lock = get_bytelock(addr);

      // If I have the write lock, record in redo log, return
      if (lock->owner == tx->id) {
          tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
          return;
      }

      // get the write lock, with timeout
      while (!bcas32(&(lock->owner), 0u, tx->id))
          if (++tries > ACQUIRE_TIMEOUT)
              tx->tmabort(tx);

      // log the lock, drop any read locks I have
      tx->w_bytelocks.insert(lock);
      lock->reader[tx->id-1] = 0;

      // wait (with timeout) for readers to drain out
      // (read 4 bytelocks at a time)
      volatile uint32_t* lock_alias = (volatile uint32_t*)&lock->reader[0];
      for (int i = 0; i < 15; ++i) {
          tries = 0;
          while (lock_alias[i] != 0)
              if (++tries > DRAIN_TIMEOUT)
                  tx->tmabort(tx);
      }

      // record in redo log
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
  }

  /**
   *  ByteEagerRedo unwinder:
   */
  stm::scope_t*
  ByteEagerRedo::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      // release write locks, then read locks
      foreach (ByteLockList, i, tx->w_bytelocks)
          (*i)->owner = 0;
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      // reset lists
      tx->r_bytelocks.reset();
      tx->w_bytelocks.reset();
      tx->writes.reset();

      // randomized exponential backoff
      exp_backoff(tx);

      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  ByteEagerRedo in-flight irrevocability:
   */
  bool
  ByteEagerRedo::irrevoc(STM_IRREVOC_SIG(,))
  {
      return false;
  }

  /**
   *  Switch to ByteEagerRedo:
   */
  void
  ByteEagerRedo::onSwitchTo() {
  }
}

namespace stm {
  /**
   *  ByteEagerRedo initialization
   */
  template<>
  void initTM<ByteEagerRedo>()
  {
      // set the name
      stms[ByteEagerRedo].name      = "ByteEagerRedo";

      // set the pointers
      stms[ByteEagerRedo].begin     = ::ByteEagerRedo::begin;
      stms[ByteEagerRedo].commit    = ::ByteEagerRedo::commit_ro;
      stms[ByteEagerRedo].read      = ::ByteEagerRedo::read_ro;
      stms[ByteEagerRedo].write     = ::ByteEagerRedo::write_ro;
      stms[ByteEagerRedo].rollback  = ::ByteEagerRedo::rollback;
      stms[ByteEagerRedo].irrevoc   = ::ByteEagerRedo::irrevoc;
      stms[ByteEagerRedo].switcher  = ::ByteEagerRedo::onSwitchTo;
      stms[ByteEagerRedo].privatization_safe = true;
  }
}
