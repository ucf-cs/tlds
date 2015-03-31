/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/***  This file declares the methods that install a new algorithm */

#ifndef INST_HPP__
#define INST_HPP__

#include <stm/config.h>
#include <common/platform.hpp>

namespace stm
{
  /*** forward declare to avoid extra dependencies */
  class TxThread;

  /*** actually make all threads use the new algorithm */
  void install_algorithm(int new_alg, TxThread* tx);

  /*** make just this thread use a new algorith (use in ctors) */
  void install_algorithm_local(int new_alg, TxThread* tx);

} // namespace stm

#endif // INST_HPP__
