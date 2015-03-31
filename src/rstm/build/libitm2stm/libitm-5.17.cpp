/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

// 5.17 User registered commit and undo actions

#include <cassert>
#include "libitm.h"
#include "Transaction.h"
#include "Scope.h"

inline void
_ITM_transaction::registerOnAbort(_ITM_userUndoFunction f, void* arg) {
    inner()->registerOnAbort(f, arg);
}

inline void
_ITM_transaction::registerOnCommit(_ITM_userCommitFunction f,
                                  _ITM_transactionId_t tid, void* arg)
{
    Node* scope = inner();
    while (scope->getId() > tid)
        scope = inner();

    assert(scope->getId() == tid && "asked for a scope that doesn't exist");
    scope->registerOnCommit(f, arg);
}

void
_ITM_addUserCommitAction(_ITM_transaction* td, _ITM_userCommitFunction f,
                         _ITM_transactionId_t tid, void* args) {
    td->registerOnCommit(f, tid, args);
}

void
_ITM_addUserUndoAction(_ITM_transaction* td, _ITM_userUndoFunction f,
                       void* arg) {
    td->registerOnAbort(f, arg);
}

void
_ITM_dropReferences(_ITM_transaction*, void*, size_t)
{
}
