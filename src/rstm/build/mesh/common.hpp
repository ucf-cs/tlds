/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  common.hpp
 *
 *  Global definitions.
 */

#ifndef MESH_CONFIG_HPP__
#define MESH_CONFIG_HPP__

#include <iostream>
    using std::cerr;
#include <cassert>

#include "common/platform.hpp"
#include "common/ThreadLocal.hpp"
#include "lock.hpp"  // pthread locks

#ifdef __APPLE__
inline void* memalign(size_t alignment, size_t size)
{
    return malloc(size);
}
#elif defined(sparc) && defined(sun)
#   include <stdlib.h>
#else
#   include <malloc.h>     // some systems need this for memalign()
#endif

extern int num_points;              // number of points
extern int num_workers;             // threads
extern bool output_incremental;     // dump edges as we go along
extern bool output_end;             // dump edges at end
extern bool verbose;                // print stats
    // verbose <- (output_incremental || output_end)

static const int MAX_WORKERS = 32;

extern d_lock io_lock;
extern unsigned long long start_time;
extern unsigned long long last_time;


/// We piggyback on the libstm configuration in order to chose a thread-local
/// storage mechanism.
class thread;
extern THREAD_LOCAL_DECL_TYPE(thread*) currentThread;

#ifdef CGL
#   define BEGIN_TRANSACTION(TYPE) {with_lock tx_cs(io_lock);
#   define END_TRANSACTION }
#endif

#ifdef ITM
#   include <new>
    [[transaction_safe]]
    void* operator new (std::size_t size) throw (std::bad_alloc);
    [[transaction_safe]] void operator delete(void* ptr) throw();
#   define BEGIN_TRANSACTION(TYPE)              \
    currentThread->enter_transaction();         \
    currentThread->erase_buffered_output();     \
    __transaction [[TYPE]] {
#   define END_TRANSACTION }                   \
        currentThread->dump_buffered_output(); \
        currentThread->leave_transaction();
#   define TRANSACTION_SAFE [[transaction_safe]]
#   define TRANSACTION_PURE [[transaction_pure]]
#   define TRANSACTION_WAIVER __transaction [[waiver]]

#include "itm.h"

#   define  SYS_INIT                   _ITM_initializeProcess
#   define  THREAD_INIT                _ITM_initializeThread
#   define  THREAD_SHUTDOWN            _ITM_finalizeThread
#   define  SYS_SHUTDOWN               _ITM_finalizeProcess

#else

#   define TRANSACTION_SAFE
#   define TRANSACTION_PURE
#   define TRANSACTION_WAIVER

#   define  SYS_INIT()
#   define  THREAD_INIT()
#   define  THREAD_SHUTDOWN()
#   define  SYS_SHUTDOWN()

#endif

#endif // MESH_CONFIG_HPP__
