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
 *  ByEAU Implementations
 *
 *    ByEAU means ByteEager with support for remotely aborting other threads
 *    when a conflict is detected.  Our hope with this framework is to model
 *    the behavior of BEHTM systems (specifically, their requestor wins)
 *    contention management policy.
 *
 *    This STM is templated.  The file implements ByEAU and ByEAUFCM.  The
 *    suffix of the name indicates how contention is managed.
 *
 *    ByEAU uses "Aggressive" conflict management.  This models requestor wins
 *    exactly: when A detects a conflict with B, A aborts B.
 *
 *    ByEAUFCM assigns each transaction a timestamp at begin time.  This is
 *    overly expensive, especially since we use a single shared counter.  FCM
 *    then uses this time as in Bobba's ISCA 2007 paper to establish guidelines
 *    for who wins in any conflict.
 */

#include "../profiling.hpp"
#include "../cm.hpp"
#include "algs.hpp"

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::ByteLockList;
using stm::bytelock_t;
using stm::get_bytelock;
using stm::threads;
using stm::UndoLogEntry;


/**
 *  Supporting #defines for tracking thread liveness/deadness
 */
#define TX_ACTIVE     0
#define TX_ABORTED    1

/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  template <class CM>
  struct ByEAU_Generic
  {
      static void Initialize(int id, const char* name);

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
   *  ByEAU initialization
   */
  template <class CM>
  void
  ByEAU_Generic<CM>::Initialize(int id, const char* name)
  {
      // set the name
      stm::stms[id].name      = name;

      // set the pointers
      stm::stms[id].begin     = ByEAU_Generic<CM>::begin;
      stm::stms[id].commit    = ByEAU_Generic<CM>::commit_ro;
      stm::stms[id].read      = ByEAU_Generic<CM>::read_ro;
      stm::stms[id].write     = ByEAU_Generic<CM>::write_ro;
      stm::stms[id].rollback  = ByEAU_Generic<CM>::rollback;
      stm::stms[id].irrevoc   = ByEAU_Generic<CM>::irrevoc;
      stm::stms[id].switcher  = ByEAU_Generic<CM>::onSwitchTo;
      stm::stms[id].privatization_safe = true;
  }

  /**
   *  ByEAU_Generic begin:
   */
  template <class CM>
  bool
  ByEAU_Generic<CM>::begin(TxThread* tx)
  {
      // mark self alive
      tx->alive = TX_ACTIVE;
      // notify the CM
      CM::onBegin(tx);
      // NB: allocator call at end since CM may block
      tx->allocator.onTxBegin();
      return false;
  }

  /**
   *  ByEAU_Generic commit (read-only):
   */
  template <class CM>
  void
  ByEAU_Generic<CM>::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // read-only... release read locks
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      // notify CM
      CM::onCommit(tx);

      // reset lists
      tx->r_bytelocks.reset();
      OnReadOnlyCommit(tx);
  }

  /**
   *  ByEAU_Generic commit (writing context):
   *
   *    Since this is ByteEager, we just drop the locks to commit, regardless of
   *    the CM policy.
   */
  template <class CM>
  void
  ByEAU_Generic<CM>::commit_rw(STM_COMMIT_SIG(tx,))
  {
      // release write locks, then read locks
      foreach (ByteLockList, i, tx->w_bytelocks)
          (*i)->owner = 0;
      foreach (ByteLockList, i, tx->r_bytelocks)
          (*i)->reader[tx->id-1] = 0;

      // notify CM
      CM::onCommit(tx);

      // clean-up
      tx->r_bytelocks.reset();
      tx->w_bytelocks.reset();
      tx->undo_log.reset();

      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  ByEAU_Generic read (read-only transaction)
   */
  template <class CM>
  void*
  ByEAU_Generic<CM>::read_ro(STM_READ_SIG(tx,addr,))
  {
      bytelock_t* lock = get_bytelock(addr);

      // If I don't have a read lock, get one
      if (lock->reader[tx->id-1] == 0) {
          // first time read, log this location
          tx->r_bytelocks.insert(lock);
          // mark my lock byte
          lock->set_read_byte(tx->id-1);
      }

      // abort the owner and wait until it cleans up
      while (uint32_t owner = lock->owner) {
          // only abort owner if CM says it's ok
          if (CM::mayKill(tx, owner - 1))
              threads[owner-1]->alive = TX_ABORTED;
          else
              tx->tmabort(tx);
          // NB: must have liveness check in the spin, since we may have read
          //     locks
          if (tx->alive == TX_ABORTED)
              tx->tmabort(tx);
      }

      // do the read
      CFENCE;
      void* result = *addr;
      CFENCE;

      // check for remote abort
      if (tx->alive == TX_ABORTED)
          tx->tmabort(tx);
      return result;
  }

  /**
   *  ByEAU_Generic read (writing transaction)
   */
  template <class CM>
  void*
  ByEAU_Generic<CM>::read_rw(STM_READ_SIG(tx,addr,))
  {
      bytelock_t* lock = get_bytelock(addr);

      // skip instrumentation if I am the writer
      if (lock->owner != tx->id) {
          // make sure I have a read lock
          if (lock->reader[tx->id-1] == 0) {
              // first time read, log this location
              tx->r_bytelocks.insert(lock);
              // mark my lock byte
              lock->set_read_byte(tx->id-1);
          }

          // abort the owner and wait until it cleans up
          while (uint32_t owner = lock->owner) {
              if (CM::mayKill(tx, owner - 1))
                  threads[owner-1]->alive = TX_ABORTED;
              else
                  tx->tmabort(tx);
              // NB: again, need liveness check
              if (tx->alive == TX_ABORTED)
                  tx->tmabort(tx);
          }
      }

      // do the read
      CFENCE;
      void* result = *addr;
      CFENCE;

      // check for remote abort
      if (tx->alive == TX_ABORTED)
          tx->tmabort(tx);
      return result;
  }

  /**
   *  ByEAU_Generic write (read-only context)
   */
  template <class CM>
  void
  ByEAU_Generic<CM>::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      bytelock_t* lock = get_bytelock(addr);

      // abort current owner, wait for release, then acquire
      while (true) {
          // abort the owner if there is one
          if (uint32_t owner = lock->owner)
              // must get permission from CM, else abort self to prevent deadlock
              if (CM::mayKill(tx, owner - 1))
                  threads[owner-1]->alive = TX_ABORTED;
              else
                  tx->tmabort(tx);
          // try to get ownership
          else if (bcas32(&(lock->owner), 0u, tx->id))
              break;
          // liveness check
          if (tx->alive == TX_ABORTED)
              tx->tmabort(tx);
      }

      // log the lock, drop any read locks I have
      tx->w_bytelocks.insert(lock);
      lock->reader[tx->id-1] = 0;

      // abort active readers
      for (int i = 0; i < 60; ++i)
          if (lock->reader[i] != 0) {
              // again, only abort readers with CM permission, else abort self
              if (CM::mayKill(tx, i))
                  threads[i]->alive = TX_ABORTED;
              else
                  tx->tmabort(tx);
          }

      // add to undo log, do in-place write
      tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
      STM_DO_MASKED_WRITE(addr, val, mask);

      // check for remote abort
      if (tx->alive == TX_ABORTED)
          tx->tmabort(tx);

      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  ByEAU_Generic write (writing context)
   */
  template <class CM>
  void
  ByEAU_Generic<CM>::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      bytelock_t* lock = get_bytelock(addr);

      // skip all this if I have the lock
      if (lock->owner != tx->id) {
          // abort current owner, wait for release, then acquire
          while (true) {
              // abort the owner if there is one
              if (uint32_t owner = lock->owner)
                  // need CM permission
                  if (CM::mayKill(tx, owner-1))
                      threads[owner-1]->alive = TX_ABORTED;
                  else
                      tx->tmabort(tx);
              // try to get ownership
              else if (bcas32(&(lock->owner), 0u, tx->id))
                  break;
              // liveness check
              if (tx->alive == TX_ABORTED)
                  tx->tmabort(tx);
          }
          // log the lock, drop any read locks I have
          tx->w_bytelocks.insert(lock);
          lock->reader[tx->id-1] = 0;

          // abort active readers
          for (int i = 0; i < 60; ++i)
              if (lock->reader[i] != 0) {
                  // get permission to abort reader
                  if (CM::mayKill(tx, i))
                      threads[i]->alive = TX_ABORTED;
                  else
                      tx->tmabort(tx);
              }
      }

      // add to undo log, do in-place write
      tx->undo_log.insert(UndoLogEntry(STM_UNDO_LOG_ENTRY(addr, *addr, mask)));
      STM_DO_MASKED_WRITE(addr, val, mask);

      // check for remote abort
      if (tx->alive == TX_ABORTED)
          tx->tmabort(tx);
  }

  /**
   *  ByEAU_Generic unwinder:
   *
   *    All ByEAU algorithms unwind in the same way: we run the undo log, then we
   *    release locks, notify the CM, and clean up.
   */
  template <class CM>
  stm::scope_t*
  ByEAU_Generic<CM>::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Undo the writes, while at the same time watching out for the exception
      // object.
      STM_UNDO(tx->undo_log, upper_stack_bound, except, len);

      // release write locks, then read locks
      foreach (ByteLockList, j, tx->w_bytelocks)
          (*j)->owner = 0;
      foreach (ByteLockList, j, tx->r_bytelocks)
          (*j)->reader[tx->id-1] = 0;

      // reset lists
      tx->r_bytelocks.reset();
      tx->w_bytelocks.reset();
      tx->undo_log.reset();

      CM::onAbort(tx);

      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  ByEAU_Generic in-flight irrevocability:
   */
  template <class CM>
  bool
  ByEAU_Generic<CM>::irrevoc(STM_IRREVOC_SIG(,))
  {
      return false;
  }

  /**
   *  Switch to ByEAU_Generic:
   *
   *    No algorithm leaves the ByteLock array in a nonzero state, so we
   *    don't have any overhead here.
   */
  template <class CM>
  void
  ByEAU_Generic<CM>::onSwitchTo() { }
}

// Register ByEAU initializer functions. Do this as declaratively as
// possible. Remember that they need to be in the stm:: namespace.
#define FOREACH_BYEAU(MACRO)                    \
    MACRO(ByEAU, BackoffCM)                     \
    MACRO(ByEAUHA, HyperAggressiveCM)           \
    MACRO(ByEAUFCM, FCM)                        \
    MACRO(ByEAUHour, HourglassCM)

#define INIT_BYEAU(ID, CM)                      \
    template <>                                 \
    void initTM<ID>() {                         \
        ByEAU_Generic<CM>::Initialize(ID, #ID); \
    }

namespace stm {
  FOREACH_BYEAU(INIT_BYEAU)
}

#undef FOREACH_BYEAU
#undef INIT_BYEAU
