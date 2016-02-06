//
//  Copyright (C) 2011
//  University of Rochester Department of Computer Science
//    and
//  Lehigh University Department of Computer Science and Engineering
// 
// License: Modified BSD
//          Please see the file LICENSE.RSTM for licensing information

// has the definitions for the checkpoint offsets
#include "checkpoint.h"
#include "common.h"
        
        .p2align 4,,15
        .globl _stm_itm2stm_checkpoint_restore
        ASM_DOT_TYPE(_stm_itm2stm_checkpoint_restore, @function)
_stm_itm2stm_checkpoint_restore:
        ASM_DOT_CFI_STARTPROC

        movl    %esi, %eax // grab the return value (won't touch %eax again) 

        movq    CHECKPOINT_RBP_(%rdi), %rbp // restore the stack frame to its
        movq    CHECKPOINT_RSP_(%rdi), %rsp // pre-call state

        movq    CHECKPOINT_RBX_(%rdi), %rbx
        movq    CHECKPOINT_R12_(%rdi), %r12
        movq    CHECKPOINT_R13_(%rdi), %r13
        movq    CHECKPOINT_R14_(%rdi), %r14
        movq    CHECKPOINT_R15_(%rdi), %r15

        // replace the return address on the stack
        movq    CHECKPOINT_RIP_(%rdi), %rcx
        movq    %rcx, (%rsp)
        
        ret // transfer control

        ASM_DOT_CFI_ENDPROC
        ASM_DOT_SIZE(_stm_itm2stm_checkpoint_restore, .-_stm_itm2stm_checkpoint_restore)
