/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <utility>
#include <cmath>
#include <limits.h>
#include "initializers.hpp" // init_pol_cbr
#include "policies.hpp"     // init_adapt_pol
#include "../profiling.hpp"
#include "../algs/algs.hpp"

using namespace stm;

/**
 *  This file implements a few classifiers for trying to pick which algorithm
 *  to switch to, based on some existing information from a profile and a
 *  table of previous experiments.
 */
namespace
{
  /**
   *  The CBR policies can all be characterized as follows:
   *
   * - We have a qtable full of normalized profiles
   *
   * - We have a summary_profile, which is the average of N profiles,
   *   normalized
   *
   * - For each qtable row, for each column, compute the norm_dist between
   *   the row and the summary_profile.  Take the weighted sum of the
   *   distances
   *
   * - Return the algorithm whose name appears in the qtable row with the
   *   minimum weighted sum norm_dist
   *
   *  Implicit in this description is the fact that norm_dist must be defined
   *  relative to a normalization strategy.
   */

  /*** HELPER FUNCTIONS ***/

  /**
   *  Using this norm_dist function implies that we tend to choose a qtable
   *  entry with larger number on some attribute when we have a tie in
   *  Manhattan distance.  E.g., when we get a profiled read number of 4, we
   *  may only have 2 entries in the qtable, one with a read number of 3 and
   *  the other one with a read number of 5.  In this case, we will choose
   *  the row with 5.
   */
  TM_INLINE unsigned long
  norm_dist(unsigned long a, unsigned long b)
  {
      return (a == b) ? 0 : (100*abs((int)(a-b)))/MAXIMUM(a, b);
  }

  /**
   *  A testing policy: we use this to return exactly the same algorithm as we
   *  had before a profile was run.  The reason this is implemented here is just
   *  so that we have a hook in the standard place to ensure that profiles get
   *  printed.
   */
  TM_FASTCALL uint32_t profile_nochange()
  {
      // compute the read-only ratio
      uint32_t ropct = 0;
      uint32_t txns = 0;
      uint32_t rotxns = 0;
      for (uint32_t i = 0; i < threadcount.val; ++i) {
          txns += threads[i]->num_commits;
          rotxns += threads[i]->num_ro;
      }
      ropct = (100*rotxns)/(txns + rotxns);

      // we have some profiles sitting around: use them with the NN code
      dynprof_t summary_profile;

      // average the set of profiles, in case we have more than one
      dynprof_t::doavg(summary_profile, profiles, profile_txns);
      summary_profile.dump();

      // compute the TxnRatio
      int txnratio = summary_profile.txn_time * 100 / get_nontxtime();

      int32_t  thrcount  = threadcount.val;
      printf("txn=%d, ro=%u, thr=%d, ntx=%llu\n", txnratio, ropct, thrcount,
             get_nontxtime());
      return curr_policy.PREPROFILE_ALG;
  }

  /**
   *  To implement our CBR policies, we simply declare a struct that describes
   *  how comparisons are to be performed.  Then, we can instantiate the above
   *  template using these different structs.
   */

  /**
   *  Metric for comparing the average ro+rw read of a qtable entry with the
   *  profile's ro+rw read count
   */
  struct Read
  {
      inline static bool uses_RO() { return false; }
      inline static bool uses_TxnRatio(){return false; }

      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t, int)
      {
          unsigned long a = norm_dist(p.read_ro, i->p.read_ro);
          int b = norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw);
          int c = norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          return  a + b + c;
      }
  };

  /**
   *  Metric for comparing the average writes of a qtable entry with the profile's
   *  write count
   */
  struct Write
  {
      inline static bool uses_RO() { return false; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t, int)
      {
          int a = norm_dist(p.write_nonwaw, i->p.write_nonwaw);
          int b = norm_dist(p.write_waw, i->p.write_waw);
          return a + b;
      }
  };

  /**
   *  Metric for comparing average time of a qtable entry with the profile's run
   *  time
   */
  struct Time
  {
      inline static bool uses_RO() { return false; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t, int)
      {
          return abs((int)(i->p.txn_time - p.txn_time));
      }
  };

  /**
   *  Metric for comparing the read only txn ratio of a qtable entry with the profile's
   *  run time
   *
   */
  struct RO
  {
      inline static bool uses_RO() { return true; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t&, qtable_t* i, uint32_t ropct, int)
      {
          return abs((int)(ropct - i->pct_ro));
      }
  };

  /**
   *  Metric for comparing sum of average read and writes of a qtable entry
   *  with the profile's sum of read and writes
   */
  struct RW
  {
      inline static bool uses_RO() { return false; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t, int)
      {
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          return read_dist * 2 + write_dist * 3;
      }
  };

  /**
   *  Metric for taking weighted sum of the Read and RO metrics
   */
  struct R_RO
  {
      inline static bool uses_RO() { return true; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t ropct,
                                    int)
      {
          int read_dist = norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return read_dist + ropct_dist*3;
      }
  };

  /**
   *  Metric for taking weighted sum of the AvgR and AvgTime metrics
   */
  struct R_Time
  {
      inline static bool uses_RO() { return false; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t, int)
      {
          int read_dist = norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int time_dist = norm_dist(i->p.txn_time, p.txn_time);
          return read_dist + time_dist*3;
      }
  };

  /**
   *  Metric for taking weighted sum of the AvgW and RO metrics
   */
  struct W_RO
  {
      inline static bool uses_RO() { return true; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t ropct,
                                    int)
      {
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return write_dist + ropct_dist*2;
      }
  };

  /**
   *  Metric for taking weighted sum of the AvgW and AvgTime metrics
   */
  struct W_Time
  {
      inline static bool uses_RO() { return false; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t, int)
      {
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw - i->p.write_waw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          return write_dist+ avgtime_dist*2;
      }
  };

  /**
   *  Metric for taking weighted sum of the AvgTime and RO metrics
   */
  struct Time_RO
  {
      inline static bool uses_RO() { return true; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t ropct,
                                    int)
      {
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          return ropct_dist + avgtime_dist;
      }
  };


  /**
   *  Metric for taking weighted sum of the Read, Write, and RO metrics
   */
  struct R_W_RO
  {
      inline static bool uses_RO() { return true; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t ropct,
                                    int)
      {
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return read_dist*2 + write_dist*3 + ropct_dist*6;
      }
  };

  /**
   *  Metric for taking weighted sum of the AvgR, AvgW, and AvgTime metrics
   */
  struct R_W_Time
  {
      inline static bool uses_RO() { return false; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t, int)
      {
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int time_dist = norm_dist(i->p.txn_time, p.txn_time);
          return read_dist*2 + write_dist*3 + time_dist*6;
      }
  };

  /**
   *  Metric for taking weighted sum of the AvgR, AvgTime, and RO metrics
   */
  struct R_Time_RO
  {
      inline static bool uses_RO() { return true; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t ropct,
                                    int)
      {
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int time_dist = norm_dist(i->p.txn_time, p.txn_time);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return read_dist + time_dist*3 + ropct_dist*2;
      }
  };


  /**
   *  Metric for taking weighted sum of the AvgW, AvgTime, and RO  metrics
   */
  struct W_Time_RO
  {
      inline static bool uses_RO() { return true; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t ropct,
                                    int)
      {
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return write_dist + avgtime_dist*2 + ropct_dist*2;
      }
  };

  /**
   *  Metric for taking weighted sum of the AvgR, AvgW, AvgTime, and RO metrics
   */
  struct R_W_Time_RO
  {
      inline static bool uses_RO() { return true; }
      inline static bool uses_TxnRatio() { return false; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t ropct,
                                    int)
      {
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int time_dist = norm_dist(i->p.txn_time, p.txn_time);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return read_dist*2 + write_dist*3 + time_dist*6 + ropct_dist*6;
      }
  };


  /**
   *  Metric for txn_vs_non_txn ratio
   */
  struct TxnRatio
  {
      inline static bool uses_RO() { return false; }
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t&, qtable_t* i, uint32_t,
                                    int txn_ratio)
      {
          return norm_dist(txn_ratio, i->txn_ratio);
      }
  };

  /**
   *  Metric for txn_ratio and AvgRead
   */
  struct TxnRatio_R
  {
      inline static bool uses_RO() { return false; }
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t,
                                    int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          return txnratio_dist*3 + read_dist;
      }
  };

  /**
   *  Metric for txn_ratio and avgWrite
   */
  struct TxnRatio_W
  {
      inline static bool uses_RO(){ return false;}
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t,
                                    int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          return txnratio_dist*2 + write_dist;
      }
  };

  /**
   *  Metric for txn_ratio and avg_RO
   */
  struct TxnRatio_RO
  {
      inline static bool uses_RO(){ return true;}
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t&, qtable_t* i, uint32_t ropct,
                                    int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return txnratio_dist + ropct_dist;
      }
  };

  /**
   *  Metric for txn_ratio and avg_time
   */
  struct TxnRatio_Time
  {
      inline static bool uses_RO() {return false;}
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t,
                                    int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          return txnratio_dist + avgtime_dist;
      }
  };

  /**
   *  Metric for avg_read and avg_write
   */
  struct TxnRatio_RW
  {
      inline static bool uses_RO(){return false;}
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t,
                                    int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          return txnratio_dist*6 + read_dist*2 + write_dist*3;
      }
  };

  /**
   *  Metric for TxnRatio, avg read and read only ratio
   */
  struct TxnRatio_R_RO
  {
      inline static bool uses_RO() { return true;}
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t& p, qtable_t *i,
                                    uint32_t ropct, int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return txnratio_dist*3 + read_dist + ropct_dist*3;
      }
  };

  /**
   *  Metric for txn_ratio and avg_time
   */
  struct TxnRatio_R_Time
  {
      inline static bool uses_RO() {return false;}
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t,
                                    int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          return txnratio_dist*3 + avgtime_dist*3 + read_dist;
      }
  };

  /**
   *  Metric for TxnRatio, avg write and read only ratio
   */
  struct TxnRatio_W_RO
  {
      inline static bool uses_RO() { return true; }
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t& p, qtable_t *i,
                                    uint32_t ropct, int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return txnratio_dist * 2 + write_dist + ropct_dist*2;
      }
  };


  /**
   *  Metric for txn_ratio and avg_time
   */
  struct TxnRatio_W_Time
  {
      inline static bool uses_RO() {return false;}
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i, uint32_t,
                                    int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          return txnratio_dist*2 + avgtime_dist*2 + write_dist;
      }
  };

  /**
   *  Metric for txn_ratio and avg_time
   */
  struct TxnRatio_RO_Time
  {
      inline static bool uses_RO() {return false;}
      inline static bool uses_TxnRatio() { return true; }
      inline static int32_t compare(dynprof_t& p, qtable_t* i,
                                    uint32_t ropct, int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return txnratio_dist + avgtime_dist + ropct_dist;
      }
  };

  /**
   *  Metric for TxnRatio, avg read, write read only ratio
   */
  struct TxnRatio_RW_RO
  {
      inline static bool uses_RO(){ return true; }
      inline static bool uses_TxnRatio() { return true; }
      inline static uint32_t compare(dynprof_t& p, qtable_t *i,
                                     uint32_t ropct, int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return txnratio_dist*6 + ropct_dist*6 + read_dist*2 + write_dist*3;
      }
  };

  /**
   *  Metric for TxnRatio, avg read, write read only ratio
   */
  struct TxnRatio_RW_Time
  {
      inline static bool uses_RO(){ return true; }
      inline static bool uses_TxnRatio() { return true; }
      inline static uint32_t compare(dynprof_t& p, qtable_t *i, uint32_t,
                                     int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          return txnratio_dist*6 + avgtime_dist*6 + read_dist*2 + write_dist*3;
      }
  };

  /**
   *  Metric for TxnRatio, avg read, write read only ratio
   */
  struct TxnRatio_R_RO_Time
  {
      inline static bool uses_RO(){ return true; }
      inline static bool uses_TxnRatio() { return true; }
      inline static uint32_t compare(dynprof_t& p, qtable_t *i,
                                     uint32_t ropct, int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return txnratio_dist*3 + avgtime_dist*3 + read_dist
              + ropct_dist*3;
      }
  };

  /***  Metric for TxnRatio, avg read, write read only ratio */
  struct TxnRatio_W_RO_Time
  {
      inline static bool uses_RO(){ return true; }
      inline static bool uses_TxnRatio() { return true; }
      inline static uint32_t compare(dynprof_t& p, qtable_t *i,
                                     uint32_t ropct, int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return txnratio_dist*2 + avgtime_dist*2 + ropct_dist*2 + write_dist;
      }
  };

  /*** Metric for TxnRatio, avg read, write read only ratio */
  struct TxnRatio_RW_RO_Time
  {
      inline static bool uses_RO(){ return true; }
      inline static bool uses_TxnRatio() { return true; }
      inline static uint32_t compare(dynprof_t& p, qtable_t *i,
                                     uint32_t ropct, int txn_ratio)
      {
          int txnratio_dist = norm_dist(txn_ratio, i->txn_ratio);
          int read_dist =  norm_dist(p.read_ro, i->p.read_ro)
              + norm_dist(p.read_rw_nonraw, i->p.read_rw_nonraw)
              + norm_dist(p.read_rw_raw, i->p.read_rw_raw);
          int write_dist = norm_dist(p.write_nonwaw, i->p.write_nonwaw)
              + norm_dist(p.write_waw, i->p.write_waw);
          int avgtime_dist = norm_dist(i->p.txn_time, p.txn_time);
          int ropct_dist = norm_dist(ropct, i->pct_ro);
          return txnratio_dist*6 + avgtime_dist*6 + read_dist*2
              + write_dist*3 + ropct_dist * 6;
      }
  };

  /**
   *  Common part of CBR code
   */
  template <class C>
  uint32_t cbr_tail(qtable_t& profile)
  {
      // prepare to scan through qtable
      int32_t  thrcount  = threadcount.val; // cache val since it is volatile
      uint32_t best_alg  = 0;               // our choice of algorithm
      int32_t  distance  = 0x7FFFFFFF;     // distance metric
      // go through the qtable, check each row
      foreach (MiniVector<qtable_t>, i, (*qtbl[thrcount])) {
          // we no longer have to check if the thread count matches as the
          // qtable we are using is sure to have only entries for the correct
          // thread count

          int32_t metric_val = 0xFFFFFFFF;
          metric_val = C::compare(profile.p, i, profile.pct_ro, profile.txn_ratio);

          // if distance smaller or distance same && throughput larger,
          // pick this alg
          if (metric_val < distance) {
              distance = metric_val;
              best_alg = i->alg_name;
          }
      }
      return best_alg;
  }

  /**
   *  This policy compares the read-only ratio of the current workload to the
   *  read-only ratios of entries in our qtable, using a nearest neighbor
   *  distance.  Note that it does not use profiles at all.
   */
  TM_FASTCALL uint32_t pol_CBR_RO()
  {
      // compute the read-only ratio
      uint32_t txns = 0, rotxns = 0;
      qtable_t q;
      for (uint32_t i = 0; i < threadcount.val; ++i) {
          txns += threads[i]->num_commits;
          rotxns += threads[i]->num_ro;
      }

      q.pct_ro = (100*rotxns)/(txns + rotxns);
      return cbr_tail<RO>(q);
  }

  /**
   *  This template provides the functionality we need for doing a
   *  nearest-neighbor classification where we use as input the summary profile,
   *  and scan through the qtable to find the most representative row
   */
  template <class C>
  TM_FASTCALL uint32_t cbr_nn()
  {
      // compute the read-only ratio... not sure if a simple Dead-Code
      // Eliminator will handle this, so we use some template stuff to guide
      // when the code runs.
      qtable_t q;
      if (C::uses_RO()) {
          uint32_t txns = 0;
          uint32_t rotxns = 0;
          for (uint32_t i = 0; i < threadcount.val; ++i) {
              txns += threads[i]->num_commits;
              rotxns += threads[i]->num_ro;
          }
          q.pct_ro = (100*rotxns)/(txns + rotxns);
      }

      // Average all the profiles we've collected
      dynprof_t::doavg(q.p, profiles, profile_txns);
      q.p.dump();

      // compute the TxnRatio
      if (C::uses_TxnRatio()) {
          q.txn_ratio = q.p.txn_time * 100 / get_nontxtime();
      }

      // get the result
      return cbr_tail<C>(q);
  }

} // namespace { }

namespace stm
{
  /**
   *  This rather ugly bit of code initializes all of our CBR policies, so that
   *  we don't need them to be visible outside of this file
   */
#define LOCAL_HELPER(pol, name)                                         \
  init_adapt_pol(CAT2(CBR_,pol), Ticket, 16, 2048, true, true, true,    \
                 cbr_nn<pol>, name)
  void init_pol_cbr()
  {
      // CBR policies that use profiles... all of these are handled by using the
      // cbr_nn template, instantiated with the above structs
      LOCAL_HELPER(Read, "CBR_Read");
      LOCAL_HELPER(Write, "CBR_Write");
      LOCAL_HELPER(Time, "CBR_Time");
      LOCAL_HELPER(RO, "CBR_RO");
      LOCAL_HELPER(RW, "CBR_RW");
      LOCAL_HELPER(R_RO, "CBR_R_RO");
      LOCAL_HELPER(R_Time, "CBR_R_Time");
      LOCAL_HELPER(W_RO, "CBR_W_RO");
      LOCAL_HELPER(W_Time, "CBR_W_Time");
      LOCAL_HELPER(Time_RO, "CBR_Time_RO");
      LOCAL_HELPER(R_W_RO, "CBR_R_W_RO");
      LOCAL_HELPER(R_W_Time, "CBR_R_W_Time");
      LOCAL_HELPER(R_Time_RO, "CBR_R_Time_RO");
      LOCAL_HELPER(W_Time_RO, "CBR_W_Time_RO");
      LOCAL_HELPER(R_W_Time_RO, "CBR_R_W_Time_RO");
      LOCAL_HELPER(TxnRatio, "CBR_TxnRatio");
      LOCAL_HELPER(TxnRatio_R, "CBR_TxnRatio_R");
      LOCAL_HELPER(TxnRatio_W, "CBR_TxnRatio_W");
      LOCAL_HELPER(TxnRatio_RO, "CBR_TxnRatio_RO");
      LOCAL_HELPER(TxnRatio_Time, "CBR_TxnRatio_Time");
      LOCAL_HELPER(TxnRatio_RW, "CBR_TxnRatio_RW");
      LOCAL_HELPER(TxnRatio_R_RO, "CBR_TxnRatio_R_RO");
      LOCAL_HELPER(TxnRatio_R_Time, "CBR_TxnRatio_R_Time");
      LOCAL_HELPER(TxnRatio_W_RO, "CBR_TxnRatio_W_RO");
      LOCAL_HELPER(TxnRatio_W_Time, "CBR_TxnRatio_W_Time");
      LOCAL_HELPER(TxnRatio_RO_Time, "CBR_TxnRatio_RO_Time");
      LOCAL_HELPER(TxnRatio_RW_RO, "CBR_TxnRatio_RW_RO");
      LOCAL_HELPER(TxnRatio_RW_Time, "CBR_TxnRatio_RW_Time");
      LOCAL_HELPER(TxnRatio_R_RO_Time, "CBR_TxnRatio_R_RO_Time");
      LOCAL_HELPER(TxnRatio_W_RO_Time, "CBR_TxnRatio_W_RO_Time");
      LOCAL_HELPER(TxnRatio_RW_RO_Time, "CBR_TxnRatio_RW_RO_Time");

      // CBR policies without profiles... there is only one such policy
      init_adapt_pol(CBR_RO, TML, 16, 2048, false, true, true,
                     pol_CBR_RO, "CBR_RO");

      // Profile policies without CBR... there is only one such policy
      init_adapt_pol(PROFILE_NOCHANGE, NOrec, 16, 2048, true, false, true,
                     profile_nochange, "PROFILE_NOCHANGE");
  }

#undef LOCAL_HELPER

} // namespace stm
