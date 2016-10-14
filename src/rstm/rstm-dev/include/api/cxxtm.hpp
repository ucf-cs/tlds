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
 *  Strictly speaking, any TM-supporting C++ compiler will not need this
 *  file.  However, if we want a program to be compilable with our library
 *  API, but also compilable with a TM C++ compiler, then we need to map the
 *  library calls to the calls the TM compiler expects.
 *
 *  NB: Right now this only works with the Intel compiler
 */

#ifndef STM_API_CXXTM_HPP
#define STM_API_CXXTM_HPP

// The prototype icc stm compiler version 4.0 doesn't understand transactional
// malloc and free without some help. The ifdef guard could be more intelligent.
#if defined(__ICC)
extern "C" {
    [[transaction_safe]] void* malloc(size_t) __THROW;
    [[transaction_safe]] void free(void*) __THROW;
}
#endif

#define TM_CALLABLE         [[transaction_safe]]

#define TM_BEGIN(TYPE)      __transaction [[TYPE]] {
#define TM_END              }

#define TM_WAIVER           __transaction [[waiver]]

#define TM_GET_THREAD()
#define TM_ARG
#define TM_ARG_ALONE
#define TM_PARAM
#define TM_PARAM_ALONE

#define TM_READ(x) (x)
#define TM_WRITE(x, y) (x) = (y)

namespace stm
{
  /**
   *  Set the current STM algorithm/policy.  This should be called at the
   *  beginning of each program phase
   */
  void set_policy(const char*);

  /***  Report the algorithm name that was used to initialize libstm */
  const char* get_algname();
}

#if defined(ITM) || defined(ITM2STM)
#include <common/platform.hpp> // nop()

#define  TM_SYS_INIT                   _ITM_initializeProcess
#define  TM_THREAD_INIT                _ITM_initializeThread
#define  TM_THREAD_SHUTDOWN            _ITM_finalizeThread
#define  TM_SYS_SHUTDOWN               _ITM_finalizeProcess
#define  TM_ALLOC                      malloc
#define  TM_FREE                       free
#if defined(ITM2STM)
#define  TM_SET_POLICY(P)              stm::set_policy(P)
#define  TM_GET_ALGNAME()              stm::get_algname()
#elif defined(ITM)
#define  TM_SET_POLICY(P)
#define  TM_GET_ALGNAME()              "icc builtin libitm.a"
#endif
#define  TM_BEGIN_FAST_INITIALIZATION  nop
#define  TM_END_FAST_INITIALIZATION    nop
#else
#error "We're not prepared for your implementation of the C++ TM spec."
#endif

#endif // API_CXXTM_HPP
