/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef TMLINLINE_HPP__
#define TMLINLINE_HPP__

/**
 *  In order to support inlining of TML instrumentation, we must make some
 *  metadata and implementation code visible in this file.  It is provided
 *  below:
 */

#include "algs.hpp"

namespace stm
{
  /**
   *  TML requires this to be called after every read
   */
  inline void afterread_TML(TxThread* tx)
  {
      CFENCE;
      if (__builtin_expect(timestamp.val != tx->start_time, false))
          tx->tmabort(tx);
  }

  /**
   *  TML requires this to be called before every write
   */
  inline void beforewrite_TML(TxThread* tx) {
      // acquire the lock, abort on failure
      if (!bcasptr(&timestamp.val, tx->start_time, tx->start_time + 1))
          tx->tmabort(tx);
      ++tx->start_time;
      tx->tmlHasLock = true;
  }

} // namespace stm

#endif // TMLINLINE_HPP__
