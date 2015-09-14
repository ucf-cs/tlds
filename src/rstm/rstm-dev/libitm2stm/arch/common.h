/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_ARCH_COMMON_H
#define STM_ITM2STM_ARCH_COMMON_H

#define ITM_SAVE_LIVE_VARIABLES 0x04
#define ITM_ABORT_FLAGS 0x18

#if defined(__APPLE__)
#   define ASM_DOT_TYPE(S, T)
#   define ASM_DOT_SIZE(S, T)
#   define ASM_DOT_CFI_STARTPROC
#   define ASM_DOT_CFI_ENDPROC
#   define ASM_DOT_CFI_OFFSET(S, T)
#   define ASM_DOT_CFI_DEF_CFO_OFFSET(S)
#else
#   define ASM_DOT_TYPE(S, T)            .type S, T
#   define ASM_DOT_SIZE(S, T)            .size S, T
#   define ASM_DOT_CFI_STARTPROC         .cfi_startproc
#   define ASM_DOT_CFI_ENDPROC           .cfi_endproc
#   define ASM_DOT_CFI_OFFSET(S, T)      .cfi_offset S, T
#   define ASM_DOT_CFI_DEF_CFO_OFFSET(S) .cfi_def_cfa_offset S
#endif

#endif // STM_ITM2STM_ARCH_COMMON_H
