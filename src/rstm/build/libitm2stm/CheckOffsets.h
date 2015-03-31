/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_CHECK_OFFSETS_H
#define STM_ITM2STM_CHECK_OFFSETS_H

// -----------------------------------------------------------------------------
// Defines some static checking functionality so that we can verify that our ASM
// offsets are valid. They could be invalidated based on struct layout
// changes. The correct offsets can be found by making the fake_begin target,
// and examining the asm.
//
// In reality, offsets.h should be a configure-time built header, but we don't
// really have the resources set up for that at the moment.
// -----------------------------------------------------------------------------

#include "offsets.h" // platform-specific implementation

#ifndef __GXX_EXPERIMENTAL_CXX0X__
#define FUNC2(x,y) x##y
#define FUNC1(x,y) FUNC2(x,y)
#define FUNC(x) FUNC1(x,__COUNTER__)
#define ASSERT_OFFSET(L, R) \
    typedef char FUNC(OFFSET_CHECK_FAILED)[(L == R) ? 1 : -1];
#else
#define ASSERT_OFFSET(L, R) static_assert(L == R, "Offset check failed.")
#endif

#endif // STM_ITM2STM_CHECK_OFFSETS_H
