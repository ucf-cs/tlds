/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_ARCH_X86_TYPE_ALIGNMENTS_H
#define STM_ITM2STM_ARCH_X86_TYPE_ALIGNMENTS_H

namespace itm2stm {
// Abstracts some platform-specific alignment issues. Each arch directory
// implements this class appropriately to tell the barrier code if a particular
// type is guaranteed to be aligned on the platform.
template <typename T>
struct Aligned {
    enum { value = false };
};
}

#endif // STM_ITM2STM_ARCH_X86_TYPE_ALIGNMENTS_H
