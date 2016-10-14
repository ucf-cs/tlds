/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_EXTRAS_H
#define STM_ITM2STM_EXTRAS_H

// These functions are not part of the ABI, but we want them in the library so
// that clients can use transactional memory allocation.

namespace itm2stm {
void*            new_wrapper(size_t sz)    asm("_Znwj._$TXN");
void             delete_wrapper(void *ptr) asm("_ZdlPv._$TXN");
extern "C" void* malloc_wrapper(size_t sz) asm("malloc._$TXN");
extern "C" void  free_wrapper(void *ptr)   asm("free._$TXN");
}

#endif
