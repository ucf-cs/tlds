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
 *  ProfileApp Implementation
 *
 *    This is not a valid STM.  It exists only to provide a simple way to
 *    measure the overhead of collecting a profile, and to gather stats.  If
 *    you run a workload with ProfileApp instrumentation, you'll get no
 *    concurrency control, but the run time for each transaction will be
 *    roughly the same as what a ProfileTM transaction runtime would be.
 *
 *    We have two variants of this code, corresponding to when we count
 *    averages, and when we count maximum values.  It turns out that this is
 *    rather simple: we need only template the commit functions, so that we
 *    can aggregate statistics in two ways.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include "RedoRAWUtils.hpp"

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::dynprof_t;
using stm::profiles;
using stm::app_profiles;
using stm::WriteSetEntry;


/*** to distinguish between the two variants of this code */
#define __AVERAGE 1
#define __MAXIMUM 0

/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  /**
   *  To support both average and max without too much overhead, we are going
   *  to template the implementation.  Then, we can specialize for two
   *  different int values, which will serve as a bool for either doing AVG (1)
   *  or MAX (0)
   */
  template <int COUNTMODE>
  struct ProfileApp
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

  template <int COUNTMODE>
  void
  ProfileApp<COUNTMODE>::Initialize(int id, const char* name)
  {
      // set the name
      stm::stms[id].name      = name;

      // set the pointers
      stm::stms[id].begin     = ProfileApp<COUNTMODE>::begin;
      stm::stms[id].commit    = ProfileApp<COUNTMODE>::commit_ro;
      stm::stms[id].read      = ProfileApp<COUNTMODE>::read_ro;
      stm::stms[id].write     = ProfileApp<COUNTMODE>::write_ro;
      stm::stms[id].rollback  = ProfileApp<COUNTMODE>::rollback;
      stm::stms[id].irrevoc   = ProfileApp<COUNTMODE>::irrevoc;
      stm::stms[id].switcher  = ProfileApp<COUNTMODE>::onSwitchTo;
      stm::stms[id].privatization_safe = true;
  }

  /**
   *  Helper MACRO
   */
#define UPDATE_MAX(x,y) if((x) > (y)) (y) = (x)

  /**
   *  ProfileApp begin:
   *
   *    Start measuring tx runtime
   */
  template <int COUNTMODE>
  bool
  ProfileApp<COUNTMODE>::begin(TxThread* tx)
  {
      tx->allocator.onTxBegin();
      profiles[0].txn_time = tick();
      return false;
  }

  /**
   *  ProfileApp commit (read-only):
   *
   *    RO commit just involves updating statistics
   */
  template <int COUNTMODE>
  void
  ProfileApp<COUNTMODE>::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // NB: statically optimized version of RW code for RO case
      unsigned long long runtime = tick() - profiles[0].txn_time;

      if (COUNTMODE == __MAXIMUM) {
          // update max values: only ro_reads and runtime change in RO transactions
          UPDATE_MAX(profiles[0].read_ro, app_profiles->read_ro);
          UPDATE_MAX(runtime,           app_profiles->txn_time);
      }
      else {
          // update totals: again, only ro_reads and runtime
          app_profiles->read_ro += profiles[0].read_ro;
          app_profiles->txn_time += runtime;
      }
      app_profiles->timecounter += runtime;

      // clear the profile, clean up the transaction
      profiles[0].read_ro = 0;
      OnReadOnlyCommit(tx);
  }

  /**
   *  ProfileApp commit (writing context):
   *
   *    We need to replay writes, then update the statistics
   */
  template <int COUNTMODE>
  void
  ProfileApp<COUNTMODE>::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // run the redo log
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
      // remember write set size before clearing it
      int x = tx->writes.size();
      tx->writes.reset();

      // compute the running time and write info
      unsigned long long runtime = tick() - profiles[0].txn_time;
      profiles[0].write_nonwaw = x;
      profiles[0].write_waw -= x;

      if (COUNTMODE == __MAXIMUM) {
          // update max values
          UPDATE_MAX(profiles[0].read_ro,        app_profiles->read_ro);
          UPDATE_MAX(profiles[0].read_rw_nonraw, app_profiles->read_rw_nonraw);
          UPDATE_MAX(profiles[0].read_rw_raw,    app_profiles->read_rw_raw);
          UPDATE_MAX(profiles[0].write_nonwaw,   app_profiles->write_nonwaw);
          UPDATE_MAX(profiles[0].write_waw,      app_profiles->write_waw);
          UPDATE_MAX(runtime,                    app_profiles->txn_time);
      }
      else {
          // update totals
          app_profiles->read_ro        += profiles[0].read_ro;
          app_profiles->read_rw_nonraw += profiles[0].read_rw_nonraw;
          app_profiles->read_rw_raw    += profiles[0].read_rw_raw;
          app_profiles->write_nonwaw   += profiles[0].write_nonwaw;
          app_profiles->write_waw      += profiles[0].write_waw;
          app_profiles->txn_time       += runtime;
      }
      app_profiles->timecounter += runtime;

      // clear the profile
      profiles[0].clear();

      // finish cleaning up
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  /**
   *  ProfileApp read (read-only transaction)
   */
  template <int COUNTMODE>
  void*
  ProfileApp<COUNTMODE>::read_ro(STM_READ_SIG(,addr,))
  {
      // count the read
      ++profiles[0].read_ro;
      // read the actual value, direct from memory
      return *addr;
  }

  /**
   *  ProfileApp read (writing transaction)
   */
  template <int COUNTMODE>
  void*
  ProfileApp<COUNTMODE>::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      // check the log for a RAW hazard, we expect to miss
      WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));
      bool found = tx->writes.find(log);
      REDO_RAW_CHECK_PROFILEAPP(found, log, mask);

      // count this read, and get value from memory
      //
      // NB: There are other interesting stats when byte logging, should we
      //     record them?
      ++profiles[0].read_rw_nonraw;
      void* val = *addr;
      REDO_RAW_CLEANUP(val, found, log, mask);
      return val;
  }

  /**
   *  ProfileApp write (read-only context)
   */
  template <int COUNTMODE>
  void
  ProfileApp<COUNTMODE>::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // do a buffered write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      ++profiles[0].write_waw;
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  /**
   *  ProfileApp write (writing context)
   */
  template <int COUNTMODE>
  void
  ProfileApp<COUNTMODE>::write_rw(STM_WRITE_SIG(tx,addr,val,mask))

  {
      // do a buffered write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      ++profiles[0].write_waw;
  }

  /**
   *  ProfileApp unwinder:
   *
   *    Since this is a single-thread STM, it doesn't make sense to support
   *    abort, retry, or restart.
   */
  template <int COUNTMODE>
  stm::scope_t*
  ProfileApp<COUNTMODE>::rollback(STM_ROLLBACK_SIG(,,,))
  {
      UNRECOVERABLE("ProfileApp should never incur an abort");
      return NULL;
  }

  /**
   *  ProfileApp in-flight irrevocability:
   */
  template <int COUNTMODE>
  bool
  ProfileApp<COUNTMODE>::irrevoc(STM_IRREVOC_SIG(,))
  {
      // NB: there is no reason why we can't support this, we just don't yet.
      UNRECOVERABLE("ProfileApp does not support irrevocability");
      return false;
  }

  /**
   *  Switch to ProfileApp:
   *
   *    The only thing we need to do is make sure we have some dynprof_t's
   *    allocated for doing our logging
   */
  template <int COUNTMODE>
  void
  ProfileApp<COUNTMODE>::onSwitchTo()
  {
      if (app_profiles != NULL)
          return;

      // allocate and configure the counters
      app_profiles = new dynprof_t();

      // set all to zero, since both counting and maxing begin with zero
      app_profiles->clear();
  }
}

// Register ProfileApp initializer functions. Do this as declaratively as
// possible. Remember that they need to be in the stm:: namespace.
#define FOREACH_PROFILEAPP(MACRO)        \
    MACRO(ProfileAppAvg, __AVERAGE)      \
    MACRO(ProfileAppMax, __MAXIMUM)      \
    MACRO(ProfileAppAll, __AVERAGE)

#define INIT_PROFILEAPP(ID, MODE)                \
    template <>                                 \
    void initTM<ID>() {                         \
        ProfileApp<MODE>::Initialize(ID, #ID);   \
    }

namespace stm {
  FOREACH_PROFILEAPP(INIT_PROFILEAPP)
}

#undef FOREACH_PROFILEAPP
#undef INIT_PROFILEAPP
