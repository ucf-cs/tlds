/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef POLICIES_HPP__
#define POLICIES_HPP__

#include <stm/config.h>
#ifdef STM_CC_SUN
#include <stdio.h>
#else
#include <cstdio>
#endif

#include <inttypes.h>
#include <common/platform.hpp>
#include <stm/metadata.hpp>
#include <stm/txthread.hpp>
#include <stm/lib_globals.hpp>

namespace stm
{
  /**
   *  These define the different adaptivity policies.  A policy is a name, the
   *  starting mode, and some information about how/when to adapt.
   */
  struct pol_t
  {
      /*** the name of this policy */
      const char* name;

      /*** name of the mode that we start in */
      int   startmode;

      /*** thresholds for adapting due to aborts and waiting */
      int   abortThresh;
      int   waitThresh;

      /*** does the policy use profiles? */
      bool  isDynamic;

      /*** does the policy require a qtable? */
      bool isCBR;

      /*** does the policy have commit-based reprofiling? */
      bool isCommitProfile;

      /*** the decision policy function pointer */
      uint32_t (*TM_FASTCALL decider) ();

      /*** simple ctor, because a NULL name is a bad thing */
      pol_t() : name(""){ }
  };

  /**
   *  This describes the state of the selected policy.  This should be a
   *  singleton, but we don't bother.  There will be one of these, which we
   *  can use to tell what the current policy is that libstm is using.
   */
  struct behavior_t
  {
      // name of current policy
      uint32_t POL_ID;

      // name of current algorithm
      volatile uint32_t ALG_ID;

      // name of alg before the last profile was collected
      uint32_t PREPROFILE_ALG;

      // did we make a decision due to aborting?
      bool abort_switch;

      // so we can backoff on our thresholds when we have repeat
      // algorithim selections
      int abortThresh;
      int waitThresh;
  };

  /**
   *  Data type for holding the dynamic transaction profiles that we collect.
   *  This is pretty sloppy for now, and the 'dump' command is really not
   *  going to be important once we get out of the debug phase.  We may also
   *  determine that we need more information than we currently have.
   */
  struct dynprof_t
  {
      int      read_ro;           // calls to read_ro
      int      read_rw_nonraw;    // read_rw calls that are not raw
      int      read_rw_raw;       // read_rw calls that are raw
      int      write_nonwaw;      // write calls that are not waw
      int      write_waw;         // write calls that are waw
      int      pad;               // to put the 64-bit val on a 2-byte boundary
      uint64_t txn_time;          // txn time
      uint64_t timecounter;       // total time in transactions

      // to be clear: txn_time is either the average time for all
      // transactions, or the max time of any transaction.  timecounter is
      // the sum of all time in transactions.  timecounter only is useful for
      // profileapp, but it is very important if we want to compute nontx/tx
      // ratio when txn_time is a max value

      /**
       *  simple ctor to prevent compiler warnings
       */
      dynprof_t()
          : read_ro(0), read_rw_nonraw(0), read_rw_raw(0), write_nonwaw(0),
            write_waw(0), pad(0), txn_time(0), timecounter(0)
      {
      }

      /**
       *  Operator for copying profiles
       */
      dynprof_t& operator=(const dynprof_t* profile)
      {
          if (this != profile) {
              read_ro        = profile->read_ro;
              read_rw_nonraw = profile->read_rw_nonraw;
              read_rw_raw    = profile->read_rw_raw;
              write_nonwaw   = profile->write_nonwaw;
              write_waw      = profile->write_waw;
              txn_time       = profile->txn_time;
          }
          return *this;
      }

      /**
       *  Print a dynprof_t
       */
      void dump()
      {
          printf("Profile: read_ro=%d, read_rw_nonraw=%d, read_rw_raw=%d, "
                 "write_nonwaw=%d, write_waw=%d, txn_time=%llu\n",
                 read_ro, read_rw_nonraw, read_rw_raw, write_nonwaw,
                 write_waw, (unsigned long long)txn_time);
      }

      /**
       *  Clear a dynprof_t
       */
      void clear()
      {
          read_ro = read_rw_nonraw = read_rw_raw = 0;
          write_nonwaw = write_waw = 0;
          txn_time = timecounter = 0;
      }

      /**
       *  If we have lots of profiles, compute their average value for each
       *  field
       */
      static void doavg(dynprof_t& dest, dynprof_t* list, int num)
      {
          // zero the important fields
          dest.clear();

          // accumulate sums into dest
          for (int i = 0; i < num; ++i) {
              dest.read_ro        += list[i].read_ro;
              dest.read_rw_nonraw += list[i].read_rw_nonraw;
              dest.read_rw_raw    += list[i].read_rw_raw;
              dest.write_nonwaw   += list[i].write_nonwaw;
              dest.write_waw      += list[i].write_waw;
              dest.txn_time       += list[i].txn_time;
          }

          // compute averages
          dest.read_ro        /= num;
          dest.read_rw_nonraw /= num;
          dest.read_rw_raw    /= num;
          dest.write_nonwaw   /= num;
          dest.write_waw      /= num;
          dest.txn_time       /= num;
      }
  };

  /**
   *  This is for storing our CBR-style qtable
   *
   *    The qtable tells us for a particular workload characteristic, what
   *    algorithm did best at each thread count.
   */
  struct qtable_t
  {
      /**
       *  Selection Fields
       *
       *    NB: These fields are for choosing the output: For a given behavior,
       *        choose the algorithm that maximizes throughput.
       */

      /*** The name of the STM algorithm that produced this result */
      int alg_name;

      /**
       *  Transaction Behavior Summary
       *
       *    NB: The profile holds a characterization of the transactions of the
       *        workload, with regard to reads and writes, and the time spent
       *        on a transaction.  Depending on which variant of ProfileApp was
       *        used to create this profile, it will either hold average values,
       *        or max values.
       *
       *    NB: We assume that a summary of transactions in the single-thread
       *        execution is appropriate for the behavior of transactions in a
       *        multithreaded execution.
       */
      dynprof_t p;

      /**
       *  Workload Behavior Summary
       */

      /**
       *  The ratio of transactional work to nontransactional work
       */
      int txn_ratio;

      /**
       *  The percentage of transactions that are Read-Only
       */
      int pct_ro;

      /*** The thread count at which this result was measured */
      int thr;

      /*** really simple ctor */
      qtable_t() : pct_ro(0) { }
  };

  /*** Used in txthread to initialize the policy subsystem */
  void pol_init(const char* mode);

  /**
   *  Just like stm_name_map, we sometimes need to turn a policy name into its
   *  corresponding enum value
   */
  int pol_name_map(const char* phasename);

  /**
   *  The POLS enum lists every adaptive policy we have
   */
  enum POLS {
      // this is a "no adaptivity" policy
      Single,
      // Testing policy, to make sure profiles are working
      PROFILE_NOCHANGE,
      // the state-machine policies
      E, ER, R, X,
      // CBR without dynamic profiling
      CBR_RO,
      // CBR with dynamic profiling
      CBR_Read, CBR_Write, CBR_Time, CBR_RW,
      CBR_R_RO, CBR_R_Time, CBR_W_RO, CBR_W_Time,
      CBR_Time_RO, CBR_R_W_RO, CBR_R_W_Time, CBR_R_Time_RO,
      CBR_W_Time_RO, CBR_R_W_Time_RO, CBR_TxnRatio, CBR_TxnRatio_R,
      CBR_TxnRatio_W, CBR_TxnRatio_RO, CBR_TxnRatio_Time, CBR_TxnRatio_RW,
      CBR_TxnRatio_R_RO, CBR_TxnRatio_R_Time, CBR_TxnRatio_W_RO,
      CBR_TxnRatio_W_Time, CBR_TxnRatio_RO_Time, CBR_TxnRatio_RW_RO,
      CBR_TxnRatio_RW_Time, CBR_TxnRatio_R_RO_Time, CBR_TxnRatio_W_RO_Time,
      CBR_TxnRatio_RW_RO_Time,
      // max value... this always goes last
      POL_MAX
  };

  /**
   *  These globals are used by our adaptivity policies
   */
  extern pol_t                 pols[POL_MAX];       // describe all policies
  extern MiniVector<qtable_t>* qtbl[MAX_THREADS+1]; // hold the CBR data
  extern behavior_t            curr_policy;         // the current STM alg

  /**
   *  Helper function.  This is a terrible thing, and one we must get rid of,
   *  especially since we are calling it far more often than we should!
   */
  TM_INLINE
  inline unsigned long long get_nontxtime()
  {
      // extimate the global nontx time per transaction
      uint32_t commits = 1;
      unsigned long long nontxn_time = 0;
      for (unsigned z = 0; z < threadcount.val; z++){
          nontxn_time += threads[z]->total_nontxn_time;
          commits += threads[z]->num_commits;
          commits += threads[z]->num_ro;
      }
      commits += !commits; // if commits is 0, make it 1, without control flow
      unsigned long long ans = 1 + (nontxn_time / commits);
      return ans;
  }

  /*** used in the policies impementations to register policies */
  void init_adapt_pol(uint32_t PolicyID,   int32_t startmode,
                      int32_t abortThresh, int32_t waitThresh,
                      bool isDynamic,      bool isCBR,
                      bool isCommitProfile,  uint32_t (*decider)() TM_FASTCALL,
                      const char* name);

} // namespace stm

#endif // POLICIES_HPP__
