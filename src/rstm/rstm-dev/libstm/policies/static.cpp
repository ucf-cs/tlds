/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <signal.h>
#include "initializers.hpp"
#include "policies.hpp"
#include "../profiling.hpp"
#include "../algs/algs.hpp"

using namespace stm;

/**
 *  Here we define the static adaptivity policies.  These do not use
 *  profiles, so they are very simple.
 *
 *  NB: these are interesting from a historical perspective, in that they
 *      were part of [Spear SPAA 2011].  However, subsequent work has shown
 *      that adapting on the basis of pathology avoidance (e.g., consecutive
 *      aborts) is not a good strategy on its own, because of the possibility
 *      of Toxic Transactions [Liu Transact 2011].
 */

namespace
{
  /**
   *  This policy is suitable for workloads that need ELA semantics and support
   *  for self-abort.
   */
  TM_FASTCALL uint32_t pol_ER()
  {
      switch (curr_policy.ALG_ID) {
        case TMLLazy:   return RingSW;
        case RingSW:    return OrecELA;
        case OrecELA:   return NOrec;
        default:        return NOrecPrio;
      }
  }

  /**
   *  This policy is suitable for workloads that need ELA semantics, but that
   *  don't need self-abort.
   */
  TM_FASTCALL uint32_t pol_E()
  {
      switch (curr_policy.ALG_ID) {
        case CGL:       return TML;
        case TML:       return TMLLazy;
        case TMLLazy:   return RingSW;
        case RingSW:    return OrecELA;
        case OrecELA:   return NOrec;
        default:        return NOrecPrio;
      }
  }

  /**
   *  This policy is suitable for workloads that don't need strong semantics
   *  but do need self-abort.
   */
  TM_FASTCALL uint32_t pol_R()
  {
      switch (curr_policy.ALG_ID) {
        case TMLLazy:   return OrecEager;
        case OrecEager: return OrecLazy;
        case OrecLazy:  return OrecFair;
        case OrecFair:  break;
        default:        return NOrecPrio;
      }

      // careful... we don't want to switch out of OrecFair unless we've
      // already done several priority bumps and we're still not happy
      if ((curr_policy.ALG_ID == OrecFair) &&
          ((Self->consec_aborts / KARMA_FACTOR) < 16))
      {
          return OrecFair;
      }
      return NOrec;
  }

  /**
   *  This policy is suitable for workloads that don't need strong semantics
   *  and also don't need support for self-abort.
   */
  TM_FASTCALL uint32_t pol_X()
  {
      // compute the new algorithm
      switch (curr_policy.ALG_ID) {
        case CGL:       return OrecEager;
        case OrecEager: return OrecLazy;
        case OrecLazy:  return OrecFair;
        case OrecFair:  break;
        default:        return NOrecPrio;
      }

      // careful... we don't want to switch out of OrecFair unless we've
      // already done several priority bumps and we're still not happy
      if ((curr_policy.ALG_ID == OrecFair) &&
          ((Self->consec_aborts / KARMA_FACTOR) < 16))
      {
          return OrecFair;
      }
      return NOrec;
  }
}

namespace stm
{
  /**
   *  To avoid excessive declarations in internal.hpp, we use this function to
   *  initialize all the policies declared in this file
   */
  void init_pol_static()
  {
      // NB: Single is a system-wide hack for using a single algorithm
      //     without any adaptivity
      init_adapt_pol(Single, -1, -1, -1, false, false, false, NULL,
                     "Single Alg");

      // Initialize the other four policies
      init_adapt_pol(E, CGL, 16, 2048, false, false, false, pol_E, "E");
      init_adapt_pol(ER, TMLLazy, 16, 2048, false, false, false, pol_ER, "ER");
      init_adapt_pol(X, CGL, 16, 2048, false, false, false, pol_X, "X");
      init_adapt_pol(R, TMLLazy, 16, 2048, false, false, false, pol_R, "R");
  }

} // namespace stm

