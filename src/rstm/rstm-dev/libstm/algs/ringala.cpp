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
 *  RingALA Implementation
 *
 *    This is RingSW, extended to support ALA semantics.  We keep a
 *    thread-local filter that unions all write sets that have been posted
 *    since this transaction started, and use that filter to detect ALA
 *    conflicts on every read.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::last_complete;
using stm::timestamp;
using stm::last_init;
using stm::ring_wf;
using stm::RING_ELEMENTS;
using stm::WriteSetEntry;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct RingALA
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
      static NOINLINE void update_cf(TxThread*);
  };

  /**
   *  RingALA begin:
   */
  bool
  RingALA::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      tx->start_time = last_complete.val;
      return false;
  }

  /**
   *  RingALA commit (read-only):
   */
  void
  RingALA::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // just clear the filters
      tx->rf->clear();
      tx->cf->clear();
      OnReadOnlyCommit(tx);
  }

  /**
   *  RingALA commit (writing context):
   *
   *    The writer commit algorithm is the same as RingSW
   */
  void
  RingALA::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // get a commit time, but only succeed in the CAS if this transaction
      // is still valid
      uintptr_t commit_time;
      do {
          commit_time = timestamp.val;
          // get the latest ring entry, return if we've seen it already
          if (commit_time != tx->start_time) {
              // wait for the latest entry to be initialized
              //
              // NB: in RingSW, we wait for this entry to be complete...
              //     here we skip it, which will require us to repeat the
              //     loop... This decision should be revisited at some point
              if (last_init.val < commit_time)
                  commit_time--;
              // NB: we don't need to union these entries into CF and then
              // intersect CF with RF.  Instead, we can just intersect with
              // RF directly.  This is safe, because RF is guaranteed not to
              // change from here on out.
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

      // copy the bits over (use SSE)
      ring_wf[(commit_time + 1) % RING_ELEMENTS].fastcopy(tx->wf);

      // setting this says "the bits are valid"
      last_init.val = commit_time + 1;

      // we're committed... run redo log, then mark ring entry COMPLETE
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
      last_complete.val = commit_time + 1;

      // clean up
      tx->writes.reset();
      tx->rf->clear();
      tx->cf->clear();
      tx->wf->clear();
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  RingALA read (read-only transaction)
   *
   *    RingALA reads are like RingSTM reads, except that we must also verify
   *    that our reads won't result in ALA conflicts
   */
  void*
  RingALA::read_ro(STM_READ_SIG(tx,addr,))
  {
      // abort if this read would violate ALA
      if (tx->cf->lookup(addr))
          tx->tmabort(tx);

      // read the value from memory, log the address, and validate
      void* val = *addr;
      CFENCE;
      tx->rf->add(addr);
      // get the latest initialized ring entry, return if we've seen it already
      if (__builtin_expect(last_init.val != tx->start_time, false))
          update_cf(tx);
      return val;
  }

  /**
   *  RingALA read (writing transaction)
   */
  void*
  RingALA::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      // check the log for a RAW hazard, we expect to miss
      WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));
      bool found = tx->writes.find(log);
      REDO_RAW_CHECK(found, log, mask);

      // abort if this read would violate ALA
      if (tx->cf->lookup(addr))
          tx->tmabort(tx);

      // read the value from memory, log the address, and validate
      void* val = *addr;
      CFENCE;
      tx->rf->add(addr);
      // get the latest initialized ring entry, return if we've seen it already
      if (__builtin_expect(last_init.val != tx->start_time, false))
          update_cf(tx);

      REDO_RAW_CLEANUP(val, found, log, mask);
      return val;
  }

  /**
   *  RingALA write (read-only context)
   */
  void
  RingALA::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // buffer the write and update the filter
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      tx->wf->add(addr);
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  RingALA write (writing context)
   */
  void
  RingALA::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      tx->wf->add(addr);
  }

  /**
   *  RingALA unwinder:
   */
  stm::scope_t*
  RingALA::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      PreRollback(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      // reset lists and filters
      tx->rf->clear();
      tx->cf->clear();
      if (tx->writes.size()) {
          tx->writes.reset();
          tx->wf->clear();
      }
      return PostRollback(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  RingALA in-flight irrevocability:
   *
   *  NB: RingALA actually **must** use abort-and-restart to preserve ALA.
   */
  bool RingALA::irrevoc(STM_IRREVOC_SIG(,)) { return false; }

  /**
   *  RingALA validation
   *
   *    For every new filter, add it to the conflict filter (cf).  Then intersect
   *    the read filter with the conflict filter to identify ALA violations.
   */
  void
  RingALA::update_cf(TxThread* tx)
  {
      // get latest entry
      uintptr_t my_index = last_init.val;

      // add all new entries to cf
      for (uintptr_t i = my_index; i >= tx->start_time + 1; i--)
          tx->cf->unionwith(ring_wf[i % RING_ELEMENTS]);

      CFENCE;
      // detect ring rollover: start.ts must not have changed
      if (timestamp.val > (tx->start_time + RING_ELEMENTS))
          tx->tmabort(tx);

      // now intersect my rf with my cf
      if (tx->rf->intersect(tx->cf))
          tx->tmabort(tx);

      // wait for newest entry to be writeback-complete before returning
      while (last_complete.val < my_index)
          spin64();

      // ensure this tx doesn't look at this entry again
      tx->start_time = my_index;
  }

  /**
   *  Switch to RingALA:
   *
   *    It really doesn't matter *where* in the ring we start.  What matters is
   *    that the timestamp, last_init, and last_complete are equal.
   */
  void
  RingALA::onSwitchTo()
  {
      last_init.val = timestamp.val;
      last_complete.val = last_init.val;
  }
}

namespace stm {
  /**
   *  RingALA initialization
   */
  template<>
  void initTM<RingALA>()
  {
      // set the name
      stms[RingALA].name      = "RingALA";

      // set the pointers
      stms[RingALA].begin     = ::RingALA::begin;
      stms[RingALA].commit    = ::RingALA::commit_ro;
      stms[RingALA].read      = ::RingALA::read_ro;
      stms[RingALA].write     = ::RingALA::write_ro;
      stms[RingALA].rollback  = ::RingALA::rollback;
      stms[RingALA].irrevoc   = ::RingALA::irrevoc;
      stms[RingALA].switcher  = ::RingALA::onSwitchTo;
      stms[RingALA].privatization_safe = true;
  }
}
