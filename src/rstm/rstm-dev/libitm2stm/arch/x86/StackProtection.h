/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_ARCH_X86_STACK_PROTECTION_H
#define STM_ITM2STM_ARCH_X86_STACK_PROTECTION_H

/// This macro is meant to compute the first stack address that it is safe to
/// write to during rollback or commit. Essentially, it forms the second half of
/// a [TOP_OF_STACK, END_OF_STACK) range for filtering writes. It is
/// calling-convention and architecture specific.
///
/// The "+ 2" protects the two words corresponding to the saved caller's frame
/// address and the return address.
///
/// This particular version is for ITM's calling convention.
#define COMPUTE_PROTECTED_STACK_ADDRESS_ITM_FASTCALL(N) \
    ((void**)__builtin_frame_address(0) + 2 + ((N - 2) >= 0) ? : 0)

/// This macro is meant to compute the first stack address that it is safe to
/// write to during rollback or commit. Essentially, it forms the second half of
/// a [TOP_OF_STACK, END_OF_STACK) range for filtering writes. It is
/// calling-convention and architecture specific.
///
/// The "+ 2" protects the two words corresponding to the saved caller's frame
/// address and the return address.
///
/// This particular version is for the default calling convention.
#define COMPUTE_PROTECTED_STACK_ADDRESS_DEFAULT(N) \
    ((void**)__builtin_frame_address(0) + 2 + N)

#endif // STM_ITM2STM_ARCH_X86_STACK_PROTECTION_H
