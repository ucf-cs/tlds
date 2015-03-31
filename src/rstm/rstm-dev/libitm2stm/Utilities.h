/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_UTILITIES_H
#define STM_ITM2STM_UTILITIES_H

#include <stdint.h>

namespace itm2stm {
// -----------------------------------------------------------------------------
// We need to mask the offset bits from an address. This is the least
// significant N=log_2(sizeof(void*)) bits, which can be computed by just taking
// sizeof(void*) - 1 and treating its unsigned representation as a mask. Then we
// do a bitwise & and pick out the offset bits.
// -----------------------------------------------------------------------------
inline size_t
offset_of(const void* const address) {
    const uintptr_t MASK = static_cast<uintptr_t>(sizeof(void*) - 1);
    const uintptr_t offset = reinterpret_cast<uintptr_t>(address) & MASK;
    return static_cast<size_t>(offset);
}

// -----------------------------------------------------------------------------
// We always do word-aligned accesses into the TM ABI (with an appropriate mask
// as necessary). This drops the word-unaligned least significant bits from the
// address, if there are any. We get the least significant bits using the same
// technique as offset_of.
// -----------------------------------------------------------------------------
inline void**
base_of(const void* const address) {
    const uintptr_t MASK = ~static_cast<uintptr_t>(sizeof(void*) - 1);
    const uintptr_t base = reinterpret_cast<uintptr_t>(address) & MASK;
    return reinterpret_cast<void**>(base);
}

inline void
add_bytes(const void*& address, size_t bytes) {
    const uint8_t* temp = reinterpret_cast<const uint8_t*>(address) + bytes;
    address = reinterpret_cast<const void*>(temp);
}

inline void
add_bytes(void*& address, size_t bytes) {
    uint8_t* temp = reinterpret_cast<uint8_t*>(address) + bytes;
    address = reinterpret_cast<void*>(temp);
}

// -----------------------------------------------------------------------------
// Whenever we need to perform a transactional load or store we need a
// mask that has 0xFF in all of the bytes that we are intersted in. This
// computes a mask given an [i, j) range, where 0 <= i < j <= sizeof(void*).
//
// NB: When the parameters are compile-time constants we expect this to become a
//   simple constant in the binary when compiled with optimizations.
// -----------------------------------------------------------------------------
inline uintptr_t
make_mask(size_t i, size_t j) {
    // assert(0 <= i && i < j && j <= sizeof(void*) && "range is incorrect")
    uintptr_t mask = ~(uintptr_t)0;
    mask = mask >> (8 * (sizeof(void*) - j)); // shift 0s to the top
    mask = mask << (8 * i);                   // shift 0s into the bottom
    return mask;
}
}

#endif // STM_ITM2STM_UTILITIES_H
