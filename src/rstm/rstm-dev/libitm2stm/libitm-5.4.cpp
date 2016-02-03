/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

// 5.4  inTransaction call

#include "libitm.h"
#include "Transaction.h"
using namespace stm;

_ITM_howExecuting
_ITM_inTransaction(_ITM_transaction* td) {
    if (!td->inner())
        return outsideTransaction;
    if (td->libraryIsInevitable())
        return inIrrevocableTransaction;
    return inRetryableTransaction;
}

