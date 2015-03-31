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
#include "StackProtection.h"
#include "stm/txthread.hpp"
using namespace itm2stm;
using std::pair;

/// When we're reentering a scope, the node has been pushed onto the free list,
/// we need to take the node off that list, and then call enter using its
/// existing flags. This happens during explicit abort and cancel and for
/// conflict aborts via tmabort (libitm-5.1,5.cpp)---which call restart.
inline uint32_t
_ITM_transaction::reenter(Node* const scope) {
    assert(free_scopes_ == scope && "Invariant violation");
    free_scopes_ = free_scopes_->next_;
    return enter(scope, scope->getFlags());
}

/// Rollback requires that we do everything required to eliminate the effects of
/// the inner scope. After rollback we will need to leave the scope for it to be
/// removed from the scopes stack.
inline void
_ITM_transaction::rollback(void** stack_lower_bound) {
    // Our scope's rollback tells us the exception object range, so that the
    // rstm library can either a) avoid undo-ing to that region, or b) actually
    // redo to that region.
    //
    // The stack_lower_bound is used to protect our execution from being
    // clobbered by an undo log.
    pair<void**, size_t>& thrown = inner()->rollback(stack_lower_bound);

    // NB: In the current version of libstm We have a mismatch between the
    //     fact that the shim is using closed nesting, and the library is
    //     using subsumption. In addition to this meaning that things like
    //     nested cancels will fail, it means that we have to be careful about
    //     what we ask the library to do.
    //
    //     One thing that we *can't* do is call rollback on the RSTM
    //     descriptor unless it is an outermost scope rollback (outermost from
    //     the perspective of the shim's closed nesting), because there is
    //     hard-coded logic behind the call that expects it to be rolling back
    //     completely.
    //
    //     This means that intermediate rollbacks as a part of a nested
    //     restart or cancel will fail... no problem since we don't support
    //     them in nested context. Rollback is also called repeatedly by the
    //     shim to get to the outermost scope. If that's what's going on, we
    //     want it to succeed.
    if (--thread_handle_.nesting_depth == 0) {
        // This call *will* pop and return the scope that the library has as
        // it's scope field. It also *will* reset the nesting depth to 0, which
        // is why we can only call it once we know the depth is supposed to be
        // 0. It's not relevant that we've already set the depth to 0; that
        // behavior is to support RSTM's "library" API.
        thread_handle_.tmrollback(&thread_handle_, stack_lower_bound,
                                  thrown.first, thrown.second);
    }
}

/// Cancel is used to rollback the innermost transaction and continue execution
/// after the transaction block. It is only used to implement an explicit,
/// user-level cancel operation.
inline void
_ITM_transaction::cancel(void** stack_lower_bound) {
    rollback(stack_lower_bound);
    Node* scope = leave();
    scope->restore(a_abortTransaction | a_restoreLiveVariables);
}

/// Restart will rollback the inner scope, and restart execution using the
/// "reenter" call. This is used to both support explicit, user-level retry, and
/// as a response to a conflict (via tmabort in libstm-5.1,5.cpp).
inline void
_ITM_transaction::restart(void** stack_lower_bound) {
    rollback(stack_lower_bound);
    Node* scope = leave();
    uint32_t mode = reenter(scope);
    scope->restore(mode | a_restoreLiveVariables);
}

/// This handles explicit user-level cancel and retry calls. There is some
/// complicated logic specified in the ITM ABI spec that we need to implement
/// here, primarily to figure out where we need to restart execution.
inline void
_ITM_transaction::abort(_ITM_abortReason why, void** stack_lower_bound) {
    // CASE 4: If the reason is exceptionBlockAbort, then we are supposed to act
    //         like the previous, non-exceptionBlockAbort reason, if there is
    //         one, or TMConflict if there isn't one.
    //
    //         If the reason is something else, we remember this reason in
    //          prev_abort_reason_. This sets prev_abort_ = true as a side
    //          effect (see the union declaration an Transaction.h).
    if (why & exceptionBlockAbort)
        why = (prev_abort_) ? prev_abort_reason_ : TMConflict;
    else
        prev_abort_reason_ = why;

    // CASE 1: cancel the scope if this is a simple user abort, or if the
    //         current transaction is an exception block transaction---a
    //         condition that I don't truly understand but which is easy enough
    //         to implement.
    if (inner()->isExceptionBlock() || why & userAbort)
        cancel(stack_lower_bound); // noreturn

    assert(why & (userRetry | TMConflict) && "Should be one of these cases.");

    // find the innermost transaction that isn't an exception block
    Node* scope = inner();
    while (scope != outer_scope_) {
        if (scope->isExceptionBlock()) {
            // CASE 2: if we found an exception block, then we'll jump to it and
            //         signal abort like CASE 1, except we set the outermost
            //         scope as aborted---per spec.
            outer_scope_->setAborted(true);
            cancel(stack_lower_bound); // noreturn
        }
        // incrementally rollback and leave the scope
        rollback(stack_lower_bound);
        leave();
        scope = inner(); // updates loop variable
    }

    // CASE 3: we're restarting the top scope
    restart(stack_lower_bound); // noreturn
}

// 5.8  Aborting a transaction
void
_ITM_abortTransaction(_ITM_transaction* td, _ITM_abortReason why,
                      const _ITM_srcLocation*) {
    td->abort(why, COMPUTE_PROTECTED_STACK_ADDRESS_ITM_FASTCALL(3));
}

void
_ITM_rollbackTransaction(_ITM_transaction* td, const _ITM_srcLocation*) {
    // This is a bit of a strange ABI call, because we're going to protect the
    // stack internally, but an undo log may clobber the external stack, i.e.,
    // anything between the outermost _ITM_beginTransaction call and the call to
    // _ITM_rollbackTransaction could be undone.
    //
    // It's the only ABI call that calls rollback /without/ also restoring the
    // begin-transaction scope.
    td->rollback(COMPUTE_PROTECTED_STACK_ADDRESS_ITM_FASTCALL(2));
}
