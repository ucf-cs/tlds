/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef LIB_GLOBALS_HPP
#define LIB_GLOBALS_HPP

/**
 *  In this file, we declare functions and variables that need to be visible
 *  to many parts of the STM library, but that do not need to be visible to
 *  application code.
 */

#include <stm/config.h>
#include <stm/metadata.hpp>

namespace stm
{
  struct TxThread;
  typedef void (*AbortHandler)(TxThread*);
  void sys_init(AbortHandler conflict_abort);
  void set_policy(const char* phasename);
  void sys_shutdown();
  bool is_irrevoc(const TxThread&);
#ifdef STM_PROTECT_STACK
  void become_irrevoc(void** upper_stack_bound);
#else
  void become_irrevoc();
#endif
  void restart();
  const char* get_algname();

  extern pad_word_t  threadcount;           // threads in system
  extern TxThread*   threads[MAX_THREADS];  // all TxThreads
}

#endif // LIB_GLOBALS_HPP
