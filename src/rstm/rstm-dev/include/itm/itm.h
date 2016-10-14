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
 *  The Intel compiler exposes some of the ABI to client programmers, but they
 *  do so in an icc-specific header. Here we expose what we need from libitm.h"
 */

#ifndef RSTM_ITM_ITM_H
#define RSTM_ITM_ITM_H

#include <common/platform.hpp>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__LP64__)
#define _ITM_FASTCALL
#else
#define _ITM_FASTCALL REGPARM(2)
#endif

/**
 *  The following Initialization and finalization functions must be
 *  visible.  These are from section 5.1 of the draft spec.
 */
int  _ITM_FASTCALL _ITM_initializeProcess(void);
int  _ITM_FASTCALL _ITM_initializeThread(void);
void _ITM_FASTCALL _ITM_finalizeThread(void);
void _ITM_FASTCALL _ITM_finalizeProcess(void);

#ifdef __cplusplus
}
#endif

#endif // RSTM_ITM_ITM_H
