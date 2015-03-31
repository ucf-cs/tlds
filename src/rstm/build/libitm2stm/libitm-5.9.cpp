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
#include "StackProtection.h"
#include "stm/txthread.hpp"

inline _ITM_transaction::Node*
_ITM_transaction::leave() {
    Node* scope = inner();
    scopes_ = scope->next_;
    scope->next_ = free_scopes_;
    free_scopes_ = scope;
    outer_scope_ = (scopes_) ? outer_scope_ : NULL;
    return scope;
}

/// Handles the _ITM_commitTransaction call. Simply asks the library to
/// commit. If the library commit call actually returns, then there weren't any
/// conflicts. If it didn't return then everything was handled by tmabort.
inline void
_ITM_transaction::commit(void** protected_stack_lower_bound) {
    // This code was pilfered from <stm/api/library.hpp>.

    // Don't commit anything if we're nested... just exit this scope, this
    // hopefully respects libstm's lack of closed nesting for the moment.
    //
    // Don't pre-decerement the nesting depth, because the tmcommit call can
    // fail due to a conflict. This calls tmabort, and tmabort will fail if the
    // nesting depth is 0.
    if (thread_handle_.nesting_depth == 1)
    {
        // dispatch to the appropriate end function
        thread_handle_.tmcommit(&thread_handle_, protected_stack_lower_bound);

        // // zero scope (to indicate "not in tx")
        CFENCE;
        thread_handle_.scope = NULL;

        // record start of nontransactional time, this misses the itm2stm commit
        // and leave time for the outermost scope, but I think we're ok.
        thread_handle_.end_txn_time = tick();
    }

    // Decrement the nesting depth unconditionally here. It's needed on a nested
    // commit, as well as after the tmcommit succeeds for the outermost scope.
    --thread_handle_.nesting_depth;

    inner()->commit();
    leave(); // don't care about the returned node during a commit
}

/// Supports the ITM tryCommit operation. There isn't currently an analog to
/// this in the rstm library, so we'll fail for now.
inline bool
_ITM_transaction::tryCommit(void**) {
    assert(false && "tryCommit not yet implemented.");
    // if (rstm_try_commit)
    inner()->commit();
    leave();
    return true;
}

/// This is supposed to clear the scopes up to the specified ID, without
/// checking for conflicts. This can't be implemented by RSTM yet.
inline void
_ITM_transaction::commitToId(_ITM_transactionId_t id) {
    assert(false && "commitToId not yet implemented.");
    for (Node* scope = inner(); scope->getId() > id; scope = inner()) {
        // rstm_merge_scopes_without_aborting
        scope->commit();
        leave();
    }
}

void
_ITM_commitTransaction(_ITM_transaction* td, const _ITM_srcLocation*) {
    td->commit(COMPUTE_PROTECTED_STACK_ADDRESS_ITM_FASTCALL(2));
}

bool
_ITM_tryCommitTransaction(_ITM_transaction* td, const _ITM_srcLocation*) {
    return td->tryCommit(COMPUTE_PROTECTED_STACK_ADDRESS_ITM_FASTCALL(2));
}

void
_ITM_commitTransactionToId(_ITM_transaction* td, const _ITM_transactionId_t id,
                           const _ITM_srcLocation*) {
    td->commitToId(id);
}

