/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <iostream>
#include <fstream>
#include "policies.hpp"
#include "initializers.hpp"    // init_pol_*
#include "../algs/algs.hpp"

using namespace stm;

namespace
{
  /**
   *  Load in a qtable
   *
   *  At the risk of considerable bit-rot in the comments, we will describe the
   *  format of a .q file here:
   *
   *    Format: comma-separated value *WITH NO SPACES*
   *
   *    Fields:
   *      1 - BM             - benchmark that produced this line [ignored]
   *      2 - ALG            - algorithm name that produced the best output
   *      3 - threads        - thread count
   *      4 - read_ro        - read_ro count
   *      5 - read_rw_nonraw - read_rw_nonraw count
   *      6 - read_raw       - read_raw count
   *      7 - write_nonwaw   - write_nonwaw count
   *      8 - write_waw      - write_waw count
   *      9 - txn_time       - txn_time count
   *     10 - pct_txtime     - pct_txtime value
   *     11 - roratio        - roratio value
   */
  void load_qtable(char*& qstr)
  {
      qtable_t q;
      char bm[1024];
      char alg[1024];
      char tmp[1024];
      int count = 0;
      // read from file... skip first line, as it should be a header
      std::ifstream myf(qstr);
      if (!myf.eof())
          myf.getline(tmp, 1024);
      while (!myf.eof()) {
          // get the benchmark and ignore it.  If it is NULL, exit the loop
          myf.getline(bm, 1024, ',');
          if (!bm[0])
              break;
          count++;
          // alg name
          myf.getline(alg, 1024, ',');
          q.alg_name = stm_name_map(alg);
          // thread count
          myf.getline(tmp, 1024, ',');
          q.thr = strtol(tmp, 0, 10);
          // read_ro
          myf.getline(tmp, 1024, ',');
          q.p.read_ro = strtol(tmp, 0, 10);
          // read_rw_nonraw
          myf.getline(tmp, 1024, ',');
          q.p.read_rw_nonraw = strtol(tmp, 0, 10);
          // read_rw_raw
          myf.getline(tmp, 1024, ',');
          q.p.read_rw_raw = strtol(tmp, 0, 10);
          // write_nonwaw
          myf.getline(tmp, 1024, ',');
          q.p.write_nonwaw = strtol(tmp, 0, 10);
          // write_waw
          myf.getline(tmp, 1024, ',');
          q.p.write_waw = strtol(tmp, 0, 10);
          // txn_time
          myf.getline(tmp, 1024, ',');
          q.p.txn_time = strtol(tmp,0,10);
          // pct_txtime
          myf.getline(tmp, 1024, ',');
          q.txn_ratio = strtol(tmp,0,10);
          // roratio
          myf.getline(tmp, 1024);
          q.pct_ro = strtol(tmp,0,10);
          // if the qtable for this thread count doesn't exist, make a new one
          if (qtbl[q.thr] == NULL)
              qtbl[q.thr] = new MiniVector<qtable_t>(64);
          // put it in the qtable
          qtbl[q.thr]->insert(q);
      }

      std::cout << "Qtable Initialization:  loaded " << count << " lines from "
                << qstr << std::endl;
  }
} // namespace {}

namespace stm
{
  /**
   *  Store the STM algorithms and adaptivity policies, so we can
   *  select one at will.  The selected one is in curr_policy.
   */
  pol_t      pols[POL_MAX];
  behavior_t curr_policy;

  /*** the qtable for CBR policies */
  MiniVector<qtable_t>* qtbl[MAX_THREADS+1]  = {NULL};

  /*** Use the policies array to map a string name to a policy ID */
  int pol_name_map(const char* phasename)
  {
      for (int i = 0; i < POL_MAX; ++i)
          if (0 == strcmp(phasename, pols[i].name))
              return i;
      return -1;
  }

  /*** SUPPORT CODE FOR INITIALIZING ALL ADAPTIVITY POLICIES */

  /**
   *  This helper function lets us easily configure our STM adaptivity
   *  policies.  The idea is that an adaptive policy can get most of its
   *  configuration from the info in its starting state, and the rest of the
   *  information is easy to provide
   */
  void init_adapt_pol(uint32_t PolicyID,   int32_t startmode,
                      int32_t abortThresh, int32_t waitThresh,
                      bool isDynamic,      bool isCBR,
                      bool isCommitProfile,  uint32_t TM_FASTCALL (*decider)(),
                      const char* name)
  {
      pols[PolicyID].startmode       = startmode;
      pols[PolicyID].abortThresh     = abortThresh;
      pols[PolicyID].waitThresh      = waitThresh;
      pols[PolicyID].isDynamic       = isDynamic;
      pols[PolicyID].isCBR           = isCBR;
      pols[PolicyID].isCommitProfile = isCommitProfile;
      pols[PolicyID].decider         = decider;
      pols[PolicyID].name            = name;
  }


  /*** for initializing the adaptivity policies */
  void pol_init(const char* mode)
  {
      // call all initialization functions
      init_pol_static();
      init_pol_cbr();

      // load in the qtable here
      char* qstr = getenv("STM_QTABLE");
      if (qstr != NULL)
          load_qtable(qstr);
  }

} // namespace stm
