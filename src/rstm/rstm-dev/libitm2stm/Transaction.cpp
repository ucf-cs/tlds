/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include "libitm.h"
#include "Transaction.h"
#include "Scope.h"
#include "CheckOffsets.h"
using namespace stm;
using namespace itm2stm;

_ITM_transaction::Node::~Node() {
#if defined(NODE_NEXT_)
    ASSERT_OFFSET(offsetof(Node, next_), NODE_NEXT_);
#endif
    if (next_)
        delete next_;
}

_ITM_transaction::_ITM_transaction(TxThread& handle)
    : thread_handle_(handle),
      outer_scope_(NULL),
      scopes_(NULL),
      free_scopes_(new Node(*this)),
      next_tid_(_ITM_NoTransactionId + 1),
      prev_abort_(0) {
#if defined(TRANSACTION_INNER_)
    ASSERT_OFFSET(offsetof(_ITM_transaction, scopes_), TRANSACTION_INNER_);
#endif
#if defined(TRANSACTION_FREE_SCOPES_)
    ASSERT_OFFSET(offsetof(_ITM_transaction, free_scopes_),
                  TRANSACTION_FREE_SCOPES_);
#endif
}

_ITM_transaction::~_ITM_transaction() {
    delete scopes_;
    delete free_scopes_;
}
