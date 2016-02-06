//
//  Copyright (C) 2011
//  University of Rochester Department of Computer Science
//    and
//  Lehigh University Department of Computer Science and Engineering
// 
// License: Modified BSD
//          Please see the file LICENSE.RSTM for licensing information

// has the definitions for the checkpoint regiser offsets
#include "checkpoint.h"
// has the definitions for the _ITM_transaction and Scope offsets
#include "offsets.h"
#include "common.h"
	              
	    .text        
        .p2align 4,,15
	    .globl _ITM_beginTransaction
	    ASM_DOT_TYPE(_ITM_beginTransaction, @function)
_ITM_beginTransaction:
	    pushl	%ebp
	    movl	%esp, %ebp
	    subl	$40, %esp
	    movl	%ebx, -12(%ebp) // callee-saves EBX
	    movl	%edi, -4(%ebp)  // callee-saves EDI
	    movl	%esi, -8(%ebp)  // callee-saves ESI
	    movl	%eax, %ebx      // ebx = _ITM_transaction* td
	    movl	%edx, %edi      // edi = uint32_t flags
	    movl	TRANSACTION_INNER_(%eax), %eax
        testl   %eax, %eax
	    jne	.check_scope_aborted
.get_scope:
	    movl	TRANSACTION_FREE_SCOPES_(%ebx), %esi
	    testl	%esi, %esi
	    je	.allocate_new_scope
.checkpoint:
	    movl	NODE_NEXT_(%esi), %eax
	    movl	%eax, TRANSACTION_FREE_SCOPES_(%ebx)
	    movl	%esi, %eax // eax == checkpoint

	    // store simple registers into the returned checkpoint
        movl    %ecx, CHECKPOINT_ECX_(%eax) // use ecx as a temporary
        movl    -12(%ebp), %ecx
	    movl    %ecx, CHECKPOINT_EBX_(%eax)
        movl    -8(%ebp), %ecx
	    movl    %ecx, CHECKPOINT_ESI_(%eax)
        movl    -4(%ebp), %ecx
	    movl    %ecx, CHECKPOINT_EDI_(%eax)
	    movl    %edi, CHECKPOINT_EDX_(%eax)        

	    // store the return pc into IP
	    movl    0x4(%ebp), %ecx
	    movl    %ecx, CHECKPOINT_EIP_(%eax)

	    // store the frame pointer into BP
	    movl    0x0(%ebp), %ecx
	    movl    %ecx, CHECKPOINT_EBP_(%eax)

	    // store the stack pointer where it would be after a leave/ret
	    leal    0x8(%ebp), %ecx
	    movl    %ecx, CHECKPOINT_ESP_(%eax)

	    movl	%edi, (%esp) // flags for call
	    movl	%esi, %edx   // scope for call
	    movl	%ebx, %ecx   // transaction for call
	    call	_stm_itm2stm_transaction_enter
	    subl	$4, %esp     // pop flags for call
	    orl	$4, %eax         // a_restoreLiveVariables | enter(...)
.exit:
	    movl	-12(%ebp), %ebx // restores callee-saves ebx
	    movl	-8(%ebp), %esi  // restores callee-saves esi
	    movl	-4(%ebp), %edi  // restores callee-saves edi
	    leave
	    ret
.check_scope_aborted:
	    cmpb	$0, SCOPE_ABORTED_(%eax)
	    je	.get_scope
	    movl	$16, %eax  // return a_abortTransaction
	    jmp	.exit          // goto exit (cleans stack)
.allocate_new_scope:
	    call	_stm_itm2stm_transaction_new_node
	    movl	%eax, %esi
	    jmp	.checkpoint
	    ASM_DOT_SIZE(_ITM_beginTransaction, .-_ITM_beginTransaction)
