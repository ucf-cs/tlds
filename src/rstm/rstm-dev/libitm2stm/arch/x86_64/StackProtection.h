/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_ARCH_X86_64_STACK_PROTECTION_H
#define STM_ITM2STM_ARCH_X86_64_STACK_PROTECTION_H

// From x86_64.org's "http://www.x86-64.org/documentation/abi-0.99.pdf"
//
// Passing Once arguments are classiﬁed, the registers get assigned (in
// left-to-right order) for passing as follows:
//
// 1. If the class is MEMORY, pass the argument on the stack.
// 2. If the class is INTEGER, the next available register of the sequence %rdi,
//    %rsi, %rdx, %rcx, %r8 and %r9 is used.
// 3. If the class is SSE, the next available vector register is used, the
//    registers are taken in the order from %xmm0 to %xmm7.
// 4. If the class is SSEUP, the eightbyte is passed in the next available
//    eightbyte chunk of the last used vector register.
// 5. If the class is X87, X87UP or COMPLEX_X87, it is passed in memory


/// This macro is meant to compute the first stack address that it is safe to
/// write to during rollback or commit. Essentially, it forms the second half of
/// a [TOP_OF_STACK, END_OF_STACK) range for filtering writes. It is
/// calling-convention and architecture specific.
///
/// The "+ 2" protects the two words corresponding to the saved caller's frame
/// address and the return address.
///
/// The 6 is because I believe that 6 parameters are passed in registers (at
/// least 6 of the kinds of parameters we're concerned with in itm2stm).
///
/// This particular version is for ITM's calling convention, which I *think* is
/// equivalent to the default calling convention in x86_64.
#define COMPUTE_PROTECTED_STACK_ADDRESS_ITM_FASTCALL(N) \
    ((void**)__builtin_frame_address(0) + 2 + ((N - 6) >= 0) ? : 0)

/// This macro is meant to compute the first stack address that it is safe to
/// write to during rollback or commit. Essentially, it forms the second half of
/// a [TOP_OF_STACK, END_OF_STACK) range for filtering writes. It is
/// calling-convention and architecture specific.
///
/// The "+ 2" protects the two words corresponding to the saved caller's frame
/// address and the return address.
///
/// The 6 is because I believe that 6 parameters are passed in registers (at
/// least 6 of the kinds of parameters we're concerned with in itm2stm).
///
/// This particular version is for the default calling convention.
#define COMPUTE_PROTECTED_STACK_ADDRESS_DEFAULT(N) \
    ((void**)__builtin_frame_address(0) + 2 + ((N - 6) >= 0) ? : 0)

#endif // STM_ITM2STM_ARCH_X86_64_STACK_PROTECTION_H
