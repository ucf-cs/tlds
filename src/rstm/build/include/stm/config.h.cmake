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
 *  This is the configured header for libstm. It records all of the
 *  configuration parameters, and should be included before anything that
 *  depends on a #define that is prefixed with 'STM'
 */

#ifndef RSTM_STM_INCLUDE_CONFIG_H
#define RSTM_STM_INCLUDE_CONFIG_H

// Target processor architecture
#cmakedefine STM_CPU_X86
#cmakedefine STM_CPU_SPARC

// Target compiler
#cmakedefine STM_CC_GCC
#cmakedefine STM_CC_SUN
#cmakedefine STM_CC_LLVM

// Target OS
#cmakedefine STM_OS_LINUX
#cmakedefine STM_OS_SOLARIS
#cmakedefine STM_OS_MACOS
#cmakedefine STM_OS_WINDOWS

// The kind of build we're doing
#cmakedefine STM_O3
#cmakedefine STM_O0
#cmakedefine STM_PG

// Histogram generation
#cmakedefine STM_COUNTCONSEC_YES

// ProfileTMtrigger
#cmakedefine STM_PROFILETMTRIGGER_ALL
#cmakedefine STM_PROFILETMTRIGGER_PATHOLOGY
#cmakedefine STM_PROFILETMTRIGGER_NONE

// Configured thread-local-storage model
#cmakedefine STM_TLS_GCC
#cmakedefine STM_TLS_PTHREAD

// Configured logging granularity
#cmakedefine STM_WS_WORDLOG
#cmakedefine STM_WS_BYTELOG
#cmakedefine STM_USE_WORD_LOGGING_VALUELIST

// Configured options
#cmakedefine STM_PROTECT_STACK
#cmakedefine STM_ABORT_ON_THROW

// Defined when we want to optimize for SSE execution
#cmakedefine STM_USE_SSE

#endif // RSTM_STM_INCLUDE_CONFIG_H
