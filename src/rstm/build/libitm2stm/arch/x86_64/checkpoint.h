/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_ARCH_X86_64_CHECKPOINT_H
#define STM_ITM2STM_ARCH_X86_64_CHECKPOINT_H

#define CHECKPOINT_SIZE 8

#define CHECKPOINT_RBX_ 0
#define CHECKPOINT_RBP_ 8
#define CHECKPOINT_R12_ 16
#define CHECKPOINT_R13_ 24
#define CHECKPOINT_R14_ 32
#define CHECKPOINT_R15_ 40
#define CHECKPOINT_RIP_ 48
#define CHECKPOINT_RSP_ 56

#endif // STM_ITM2STM_ARCH_X86_64_CHECKPOINT_H
