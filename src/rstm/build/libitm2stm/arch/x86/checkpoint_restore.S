//
//  Copyright (C) 2011
//  University of Rochester Department of Computer Science
//    and
//  Lehigh University Department of Computer Science and Engineering
// 
// License: Modified BSD
//          Please see the file LICENSE.RSTM for licensing information

// Implementation is basically the same as newlib's, though I actually wrote it
// myself.         
#include "checkpoint.h"
#include "common.h"
        
        .p2align 4,,15
        .globl _stm_itm2stm_checkpoint_restore
        ASM_DOT_TYPE(_stm_itm2stm_checkpoint_restore, @function)
_stm_itm2stm_checkpoint_restore:
        ASM_DOT_CFI_STARTPROC

        movl  0x8(%esp), %eax // grab the return value (won't touch %eax again) 
        movl  0x4(%esp), %ecx // grab the buffer

        movl  CHECKPOINT_EBP_(%ecx), %ebp // restore the stack frame to its
        movl  CHECKPOINT_ESP_(%ecx), %esp // pre-call state

        pushl  CHECKPOINT_EIP_(%ecx) // push the return address (emulate call) 
                
        movl  CHECKPOINT_EBX_(%ecx), %ebx // restore ebx
        movl  CHECKPOINT_EDX_(%ecx), %edx // restore edx
        movl  CHECKPOINT_ESI_(%ecx), %esi // restore esi
        movl  CHECKPOINT_EDI_(%ecx), %edi // restore edi
        movl  CHECKPOINT_ECX_(%ecx), %ecx // restore ecx (was buffer)
        
        ret // transfer control
        
        ASM_DOT_CFI_ENDPROC
        ASM_DOT_SIZE(_stm_itm2stm_checkpoint_restore, .-_stm_itm2stm_checkpoint_restore)
