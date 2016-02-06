/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <cassert>
#include "libitm.h"
#include "StackProtection.h"
#include "stm/lib_globals.hpp"

void
_ITM_changeTransactionMode(_ITM_transaction*, _ITM_transactionState state,
                           const _ITM_srcLocation* src) {
    assert(state == modeSerialIrrevocable && "Unexpected state change request");

    // Try to use the library's internal irrevocable option. When this fails it
    // aborts, so we don't have to handle failure here.
    stm::become_irrevoc(COMPUTE_PROTECTED_STACK_ADDRESS_ITM_FASTCALL(3));
}

