/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

// 5.15  Transactional versions of memset

#include "libitm.h"
#include "Transaction.h"
#include "BlockOperations.h"
using itm2stm::block_set;

void
_ITM_memsetW(_ITM_transaction* td, void* target, int c, size_t n)
{
    block_set(td->handle(), target, static_cast<uint8_t>(c), n);
}

void
_ITM_memsetWaR(_ITM_transaction* td, void* target, int c, size_t n)
{
    block_set(td->handle(), target, static_cast<uint8_t>(c), n);
}

void
_ITM_memsetWaW(_ITM_transaction* td, void* target, int c, size_t n)
{
    block_set(td->handle(), target, static_cast<uint8_t>(c), n);
}
