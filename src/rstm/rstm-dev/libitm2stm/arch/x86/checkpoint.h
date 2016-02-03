/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_ARCH_X86_CHECKPOINT_H
#define STM_ITM2STM_ARCH_X86_CHECKPOINT_H

#define CHECKPOINT_SIZE 8

#define CHECKPOINT_EBX_ 0x0
#define CHECKPOINT_ECX_ 0x4
#define CHECKPOINT_EDX_ 0x8
#define CHECKPOINT_ESI_ 0xc
#define CHECKPOINT_EDI_ 0x10
#define CHECKPOINT_ESP_ 0x14
#define CHECKPOINT_EBP_ 0x18
#define CHECKPOINT_EIP_ 0x1c

#endif // STM_ITM2STM_ARCH_X86_CHECKPOINT_H
