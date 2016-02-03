/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_REDO_RAW_CHECK_HPP
#define STM_REDO_RAW_CHECK_HPP

#include <stm/config.h>

/**
 *  Redo-log TMs all need to perform a read-after-write check in their read_rw
 *  barriers in order to find any previous transactional writes. This is
 *  complicated when we're byte logging, because we may have written only part
 *  of a word, and be attempting to read a superset of the word bytes.
 *
 *  This file provides three config-dependent macros to assist in dealing with
 *  the case where we need to merge a read an write to get the "correct" value.
 */

#if defined(STM_WS_WORDLOG)
/**
 *  When we're word logging the RAW check is trivial. If we found the value it
 *  was returned as log.val, so we can simply return it.
 */
#define REDO_RAW_CHECK(found, log, mask) \
    if (__builtin_expect(found, false))  \
        return log.val;

/**
 *  When we're word logging the RAW check is trivial. If we found the value
 *  it was returned as log.val, so we can simply return it. ProfileApp
 *  version logs the event.
 */
#define REDO_RAW_CHECK_PROFILEAPP(found, log, mask) \
    if (__builtin_expect(found, false)) {     \
        ++profiles[0].read_rw_raw;              \
        return log.val;                       \
    }

/**
 *  No RAW cleanup is required when word logging, we can just return the
 *  unadulterated value from the log. ProfileApp version logs the event.
 */
#define REDO_RAW_CLEANUP(val, found, log, mask)

#elif defined(STM_WS_BYTELOG)

/**
 *  When we are byte logging and the writeset says that it found the address
 *  we're looking for, it could mean one of two things. We found the requested
 *  address, and the requested mask is a subset of the logged mask, or we found
 *  some of the bytes that were requested. The log.mask tells us which of the
 *  returned bytes in log.val are valid.
 *
 *  We perform a subset test (i.e., mask \in log.mask) and just return log.val
 *  if it's successful. If mask isn't a subset of log.mask we'll deal with it
 *  during REDO_RAW_CLEANUP.
 */
#define REDO_RAW_CHECK(found, log, pmask)       \
    if (__builtin_expect(found, false))         \
        if ((pmask & log.mask) == pmask)        \
            return log.val;

/**
 *  ProfileApp version logs the event.
 *
 *  NB: Byte logging exposes new possible statistics, should we record them?
 */
#define REDO_RAW_CHECK_PROFILEAPP(found, log, pmask)  \
    if (__builtin_expect(found, false)) {       \
        if ((pmask & log.mask) == pmask) {      \
            ++profiles[0].read_rw_raw;          \
            return log.val;                     \
        }                                       \
    }

/**
 *  When we're byte logging we may have had a partial RAW hit, i.e., the
 *  write log had some of the bytes that we needed, but not all.
 *
 *  Check for a partial hit. If we had one, we need to mask out the recently
 *  read bytes that correspond to the valid bytes from the log, and then merge
 *  in the logged bytes.
 */
#define REDO_RAW_CLEANUP(value, found, log, mask)               \
    if (__builtin_expect(found, false)) {                       \
        /* can't do masking on a void* */                       \
        uintptr_t v = reinterpret_cast<uintptr_t>(value);       \
        v &= ~log.mask;                                         \
        v |= reinterpret_cast<uintptr_t>(log.val) & log.mask;   \
        value = reinterpret_cast<void*>(v);                     \
    }

#else
#error "Preprocessor configuration error: STM_WS_(WORD|BYTE)LOG not defined."
#endif

#endif // STM_REDO_RAW_CHECK_HPP
