/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef MACROS_HPP__
#define MACROS_HPP__

#include <stm/config.h>

/**
 *  This file establishes a few helpful macros.  Some of these are obvious.
 *  Others are used to simplify some very redundant programming, particularly
 *  with regard to declaring STM functions and abort codes.
 */

/*** Helper Macros for concatenating tokens ***/
#define CAT2(a,b)  a ## b
#define CAT3(a,b,c)  a ## b ## c

/*** Helper Macros for turning a macro token into a string ***/
#define TO_STRING_LITERAL(arg) #arg
#define MAKE_STR(arg) TO_STRING_LITERAL(arg)

/*** max of two vals */
#define MAXIMUM(x,y) (((x)>(y))?(x):(y))

/**
 * C++ iterators can get so ugly without c++0x 'auto'.  These macros are not
 * a good idea, but it makes it much easier to write 79-column code
 */
#define foreach(TYPE, VAR, COLLECTION)                  \
    for (TYPE::iterator VAR = COLLECTION.begin(),       \
         CEND = COLLECTION.end();                       \
         VAR != CEND; ++VAR)

#define FOREACH_REVERSE(TYPE, VAR, COLLECTION)          \
    for (TYPE::iterator VAR = COLLECTION.end() - 1,     \
         CAT2(end, __LINE__) = COLLECTION.begin();      \
         VAR >= CAT2(end, __LINE__); --VAR)

/**
 *  When we use compiler-based instrumentation, support for
 *  sub-word-granularity accesses requires the individual STM read/write
 *  functions to take a mask as the third parameter.  The following macros let
 *  us inject a parameter into the function signature as needed for this
 *  purpose.
 */
#if   defined(STM_WS_BYTELOG)
#define STM_MASK(x) , x
#elif defined(STM_WS_WORDLOG)
#define STM_MASK(x)
#else
#error "Either STM_WS_BYTELOG or STM_WS_WORDLOG must be defined"
#endif

/**
 *  We need to protect the stack from "stale" writes during redo-writeback and
 *  undo-undo. This requires some platform-specific functionality to get some
 *  stack information.
 */
#ifdef STM_PROTECT_STACK
#   define STM_WHEN_PROTECT_STACK(S) S
#   define TOP_OF_ARGS(N) ((void**)__builtin_frame_address(0) + 2 + N)
#else
#   define STM_WHEN_PROTECT_STACK(S)
#endif

#ifdef STM_ABORT_ON_THROW
#   define STM_WHEN_ABORT_ON_THROW(S) S
#else
#   define STM_WHEN_ABORT_ON_THROW(S)
#endif

#if defined(STM_WS_BYTELOG)
#   define STM_READ_SIG(tx, addr, mask)       TxThread* tx, void** addr, uintptr_t mask
#   define STM_WRITE_SIG(tx, addr, val, mask) TxThread* tx, void** addr, void* val, uintptr_t mask
#else
#   define STM_READ_SIG(tx, addr, mask)       TxThread* tx, void** addr
#   define STM_WRITE_SIG(tx, addr, val, mask) TxThread* tx, void** addr, void* val
#endif

#if defined(STM_PROTECT_STACK)
#   define STM_COMMIT_SIG(tx, stack)  TxThread* tx, void** stack
#   define STM_IRREVOC_SIG(tx, stack) TxThread* tx, void** stack
#else
#   define STM_COMMIT_SIG(tx, stack)  TxThread* tx
#   define STM_IRREVOC_SIG(tx, stack) TxThread* tx
#endif

#if defined(STM_PROTECT_STACK) && defined(STM_ABORT_ON_THROW)
#   define STM_ROLLBACK_SIG(tx, stack, exception, len)                  \
    TxThread* tx, void** stack, void** exception, size_t len
#elif defined(STM_PROTECT_STACK) && !defined(STM_ABORT_ON_THROW)
#   define STM_ROLLBACK_SIG(tx, stack, exception, len)  \
    TxThread* tx, void** stack
#elif !defined(STM_PROTECT_STACK) && defined(STM_ABORT_ON_THROW)
#   define STM_ROLLBACK_SIG(tx, stack, exception, len)  \
    TxThread* tx, void** exception, size_t len
#elif !defined(STM_PROTECT_STACK) && !defined(STM_ABORT_ON_THROW)
#   define STM_ROLLBACK_SIG(tx, stack, exception, len)  TxThread* tx
#else
#   error Preprocessor if/else logic error
#endif

#endif // MACROS_HPP__
