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
#include "Transaction.h"
#include "Scope.h"

void
_ITM_registerThrownObject(_ITM_transaction* td, const void* addr, size_t len) {
    assert(!td->inner()->isExceptionBlock());
    td->inner()->setThrownObject((void**)addr, len);
}
