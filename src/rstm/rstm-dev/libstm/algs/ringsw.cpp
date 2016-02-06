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
 *  RingSW Implementation
 *
 *    This is the "single writer" variant of the RingSTM algorithm, published
 *    by Spear et al. at SPAA 2008.  There are many optimizations, based on the
 *    Fastpath paper by Spear et al. LCPC 2009.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::TxThread;
using stm::timestamp;
using stm::timestamp_max;
using stm::last_complete;
using stm::last_init;
using stm::ring_wf;
using stm::RING_ELEMENTS;
using stm::WriteSetEntry;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct RingSW {
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
      static NOINLINE void check_inflight(TxThread* tx, uintptr_t my_index);
  };

  /**
   *  RingSW begin:
   *
   *    To start a RingSW transaction, we need to find a ring entry that is
   *    writeback-complete.  In the old RingSW, this was hard.  In the new
   *    RingSW, inspired by FastPath, this is easy.
   */
  bool
  RingSW::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      // start time is when the last txn completed
      tx->start_time = last_complete.val;
      return false;
  }

  /**
   *  RingSW commit (read-only):
   */
  void
  RingSW::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // clear the filter and we are done
      tx->rf->clear();
      OnReadOnlyCommit(tx);
  }

  /**
   *  RingSW commit (writing context):
   *
   *    This is the crux of the RingSTM algorithm, and also the foundation for
   *    other livelock-free STMs.  The main idea is that we use a single CAS to
   *    transition a valid transaction from a state in which it is invisible to a
   *    state in which it is logically committed.  This transition stops the
   *    world, while the logically committed transaction replays its writes.
   */
  void
  RingSW::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // get a commit time, but only succeed in the CAS if this transaction
      // is still valid
      uintptr_t commit_time;
      do {
          commit_time = timestamp.val;
          // get the latest ring entry, return if we've seen it already
          if (commit_time != tx->start_time) {
              // wait for the latest entry to be initialized
              while (last_init.val < commit_time)
                  spin64();

              // intersect against all new entries
              for (uintptr_t i = commit_time; i >= tx->start_time + 1; i--)
                  if (ring_wf[i % RING_ELEMENTS].intersect(tx->rf))
                      tx->tmabort(tx);

              // wait for newest entry to be wb-complete before continuing
              while (last_complete.val < commit_time)
                  spin64();

              // detect ring rollover: start.ts must not have changed
              if (timestamp.val > (tx->start_time + RING_ELEMENTS))
                  tx->tmabort(tx);

              // ensure this tx doesn't look at this entry again
              tx->start_time = commit_time;
          }
      } while (!bcasptr(&timestamp.val, commit_time, commit_time + 1));

      // copy the bits over (use SSE, not indirection)
      ring_wf[(commit_time + 1) % RING_ELEMENTS].fastcopy(tx->wf);

      // setting this says "the bits are valid"
      last_init.val = commit_time + 1;

      // we're committed... run redo log, then mark ring entry COMPLETE
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
      last_complete.val = commit_time + 1;

      // clean up
      tx->writes.reset();
      tx->rf->clear();
      tx->wf->clear();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  RingSW read (read-only transaction)
   */
  void*
  RingSW::read_ro(STM_READ_SIG(tx,addr,))
  {
      // read the value from memory, log the address, and validate
      void* val = *addr;
      CFENCE;
      tx->rf->add(addr);
      // get the latest initialized ring entry, return if we've seen it already
      uintptr_t my_index = last_init.val;
      if (__builtin_expect(my_index != tx->start_time, false))
          check_inflight(tx, my_index);
      return val;
  }

  /**
   *  RingSW read (writing transaction)
   */
  void*
  RingSW::read_rw(STM_READ_SIG(tx,addr,mask))
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
   *  RingSW write (read-only context)
   */
  void
  RingSW::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // buffer the write and update the filter
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      tx->wf->add(addr);
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  RingSW write (writing context)
   */
  void
  RingSW::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      tx->wf->add(addr);
  }

  /**
   *  RingSW unwinder:
   */
  stm::scope_t*
  RingSW::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      // reset filters and lists
      tx->rf->clear();
      if (tx->writes.size()) {
          tx->writes.reset();
          tx->wf->clear();
      }
      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  RingSW in-flight irrevocability: use abort-and-restart
   */
  bool
  RingSW::irrevoc(STM_IRREVOC_SIG(tx,upper_stack_bound))
  {
      return false;
  }

  /**
   *  RingSW validation
   *
   *    check the ring for new entries and validate against them
   */
  void
  RingSW::check_inflight(TxThread* tx, uintptr_t my_index)
  {
      // intersect against all new entries
      for (uintptr_t i = my_index; i >= tx->start_time + 1; i--)
          if (ring_wf[i % RING_ELEMENTS].intersect(tx->rf))
              tx->tmabort(tx);

      // wait for newest entry to be writeback-complete before returning
      while (last_complete.val < my_index)
          spin64();

      // detect ring rollover: start.ts must not have changed
      if (timestamp.val > (tx->start_time + RING_ELEMENTS))
          tx->tmabort(tx);

      // ensure this tx doesn't look at this entry again
      tx->start_time = my_index;
  }

  /**
   *  Switch to RingSW:
   *
   *    It really doesn't matter *where* in the ring we start.  What matters is
   *    that the timestamp, last_init, and last_complete are equal.
   */
  void
  RingSW::onSwitchTo()
  {
      last_init.val = timestamp.val;
      last_complete.val = last_init.val;
  }
}

namespace stm {
  /**
   *  RingSW initialization
   */
  template<>
  void initTM<RingSW>()
  {
      // set the name
      stm::stms[RingSW].name      = "RingSW";

      // set the pointers
      stm::stms[RingSW].begin     = ::RingSW::begin;
      stm::stms[RingSW].commit    = ::RingSW::commit_ro;
      stm::stms[RingSW].read      = ::RingSW::read_ro;
      stm::stms[RingSW].write     = ::RingSW::write_ro;
      stm::stms[RingSW].rollback  = ::RingSW::rollback;
      stm::stms[RingSW].irrevoc   = ::RingSW::irrevoc;
      stm::stms[RingSW].switcher  = ::RingSW::onSwitchTo;
      stm::stms[RingSW].privatization_safe = true;
  }
}
