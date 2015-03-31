/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef API_STAMP_HPP__
#define API_STAMP_HPP__

#include <setjmp.h>
#include <cstdlib>
#include <cassert>
#include <api/library.hpp>

/**
 *  We are gradually moving to a state where STAMP will be in pure C++.
 *  Clearly we are not there yet, but currently we have STAMP set to build with
 *  g++ instead of gcc.
 *
 *  That being the case, we're going to avoid some cruft by mapping MACROs
 *  directly to c++ functions, instead of hiding code within an 'extern C'
 *  block with C-style wrapper functions.
 */

#define STM_THREAD_T             stm::TxThread
#define STM_SELF                 tm_descriptor
#define STM_STARTUP(numThread)   tm_main_startup()
#define STM_SHUTDOWN()           stm::sys_shutdown()
#define STM_NEW_THREAD()         0
#define STM_INIT_THREAD(t, id)   tm_start(&t, thread_getId())
#define STM_FREE_THREAD(t)
#define STM_RESTART()            stm::restart()

#define STM_LOCAL_WRITE_I(var, val) ({var = val; var;})
#define STM_LOCAL_WRITE_L(var, val) ({var = val; var;})
#define STM_LOCAL_WRITE_F(var, val) ({var = val; var;})
#define STM_LOCAL_WRITE_P(var, val) ({var = val; var;})

/**
 *  Special alloc for STAMP, used to detect nontransactional mallocs from
 *  within transactions
 */
inline void* tx_safe_non_tx_alloc(size_t size)
{
    return malloc(size);
}

/**
 *  Special free for STAMP, used to detect nontransactional frees from within
 *  transactions
 */
inline void tx_safe_non_tx_free(void * ptr)
{
    free(ptr);
}

/**
 *  The begin and commit instrumentation are straightforward
 */
#define STM_BEGIN_WR()                                                  \
    {                                                                   \
    jmp_buf jmpbuf_;                                                    \
    uint32_t abort_flags = setjmp(jmpbuf_);                             \
    begin(static_cast<stm::TxThread*>(STM_SELF), &jmpbuf_, abort_flags);\
    CFENCE;                                                             \
    {

#define STM_END()                                   \
    }                                               \
    commit(static_cast<stm::TxThread*>(STM_SELF));  \
    }

/*** read-only begin == read/write begin */
#define STM_BEGIN_RD() STM_BEGIN_WR()

/**
 *  tm_main_startup()
 *
 *  call before any threads try to run transactions, in order to ensure
 *  that the TM library is properly configured.
 *
 *  multiple calls are safe, since the library protects itself
 */
inline void tm_main_startup()
{
    // start the STM runtime
    stm::sys_init(NULL);

    // create a descriptor for this thread
    stm::thread_init();
}

/**
 *  tm_start(desc, id)
 *
 *  STAMP uses this during its main processing loop, once all of the
 *  threads have been created and are sitting at a barrier waiting to
 *  start. We expect it to be only called once per thread, but it is ok if
 *  it is called more than once.
 *
 *  The thread that called tm_main_startup(...) /can/, but does not have
 *  to, call this routine.
 */
inline void tm_start(stm::TxThread** desc, int id)
{
    stm::thread_init();
    // The desc parameter is an "out" parameter, so return its address
    *desc = stm::Self;
}

#endif // API_STAMP_HPP__
