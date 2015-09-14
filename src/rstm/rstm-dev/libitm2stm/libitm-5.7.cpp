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
#include "common/platform.hpp"
#include "stm/txthread.hpp"
#include "stm/lib_globals.hpp"
using namespace stm;
using namespace itm2stm;

// We depende on CGL having ID = 0
static const int CGL = 0;

/// We call the enter routine to begin a transaction. It is called both from an
/// instance of _ITM_beginTransaction, and from _ITM_transaction::reenter during
/// an abort-and-restart.
uint32_t
_ITM_transaction::enter(Node* const scope, const uint32_t flags) {
    // clear any abort reasons that are hanging around
    prev_abort_ = false;

    // initialize the scope
    scope->enter(next_tid_++, flags);

    // push the scope onto the stack
    scope->next_ = scopes_;
    scopes_ = scope;

    // Find out what the library wants to do.

    //
    // LD: following is from api/library.hpp
    //
    // This hopefully respects libstm's lack of closed nesting.
    //
    // unsigned overflow is defined
    bool irrevocable = false;
    if (thread_handle_.nesting_depth++ == 0)
    {
        // This only runs at the outermost nesting depth. Log the scope here.
        outer_scope_ = scope;

        // We must ensure that the write of the transaction's scope occurs
        // *before* the read of the begin function pointer.  on x86, a CAS is
        // faster than using WBR or xchg to achieve the ordering.  On SPARC, WBR
        // is best.
#ifdef STM_CPU_SPARC
        thread_handle_.scope = scope; WBR;
#else
        bcasptr(&thread_handle_.scope, (Node*)NULL, scope);
#endif

        // Some adaptivity mechanisms need to know nontransactional and
        // transactional time.  This code suffices, because it gets the time
        // between transactions.  If we need the time for a single transaction,
        // we need to use ProfileTM.
        if (thread_handle_.end_txn_time)
            thread_handle_.total_nontxn_time += (tick() -
                                                 thread_handle_.end_txn_time);

        // Now call the per-algorithm begin function.
        irrevocable = TxThread::tmbegin(&thread_handle_);
    }
    else {
        irrevocable = libraryIsInevitable();
    }

    // We need to tell the caller what mode we'd like to run in. If the STM
    // library has become inevitable, and there's an uninstrumented code path
    // available, we'll choose that. Otherwise run the normally instrumented
    // code.
    if (irrevocable && (flags & pr_uninstrumentedCode))
        return a_runUninstrumentedCode;

    return a_runInstrumentedCode;
}

/// Find out from the library if we have become inevitable. This hides some
/// ugly, internal libstm logic that we'd really rather not reproduce here. Not
/// only is it subject to change, but it exposes a bunch of headers that aren't
/// really public.
bool
_ITM_transaction::libraryIsInevitable() const {
    return stm::is_irrevoc(thread_handle_);
}

/// Scopes are initialized with a back-pointer to their transaction. With
/// __thread TLS this is sort of a waste of space, doing the thread-local lookup
/// of the global transaction descriptor, td, even through _ITM_getTransaction,
/// isn't really that big a deal, particularly because it's only useful on an
/// abort path. Unfortunately there's the reality that someone might be using
/// pthread TLS, where it makes a bit more difference. There aren't that many
/// scopes out there, so we won't worry about the space.
_ITM_transaction::Node::Node(_ITM_transaction& owner)
    : Scope(owner),
      next_(NULL) {
}

/// This _only_ exists so that we can give the node constructor an asm label
/// that get's used in the _ITM_beginTransaction.S code.
_ITM_transaction::Node*
_ITM_transaction::NewNode() {
    return new Node(*this);
}

/// This section of the file is only used to generate a template for the
/// _ITM_beginTransaction.S file.
#ifdef FAKE_ITM_BEGIN_TRANSACTION
#include <common/platform.hpp> // REGPARM

/// This doesn't really exist (it's always_inline), it gets inlined into
/// _FAKE_beginTransaction, which serves as the model for our manual
/// _ITM_beginTransaction.S implementations.
_ITM_transaction::Node*
_ITM_transaction::getScope() {
    Node* scope = (__builtin_expect(free_scopes_ != NULL, true)) ?
                      free_scopes_ : NewNode();
    free_scopes_ = scope->next_;
    return scope;
}

/// This sample version of _ITM_beginTransaction is just meant as a guide to
/// implementing the real _ITM_beginTransaction in a .S file.
extern void
_FAKE_checkpoint(_ITM_transaction::Node* const) asm("_FAKE_checkpoint")
    REGPARM((1));

/// Declares the fake begin transaction routine as fastcall and used.
static uint32_t
_FAKE_beginTransaction(_ITM_transaction*, uint32_t, _ITM_srcLocation*)
    _ITM_FASTCALL __attribute__((used));

/// This is what _ITM_beginTransaction is supposed to do. The real
/// _ITM_beginTransaction.S actually does this, however the need to
/// _FAKE_checkpoint means that we need to know where the callee-saves registers
/// are being stored.
///
/// We use gotos here with builtin expect to control the code layout, expecting
/// a branch-and-goto to be false suggests that the true branch should be
/// implemented as a "taken" branch instruction, which means the common case
/// code is the fast, non-taken path.
///
/// This may not matter to modern x86 processors, but it does matter on sparc's
/// Niagara2 where all branches are predicted to be not-taken.
uint32_t
_FAKE_beginTransaction(_ITM_transaction* td, uint32_t flags, _ITM_srcLocation*)
{
    _ITM_transaction::Node* scope = td->inner();
    // we want to optimize for short non-nested transactions
    if (__builtin_expect(scope != NULL, false))
        goto check_aborted;

  enter:
    scope = td->getScope();
    _FAKE_checkpoint(scope); // attribte regparm(1) means that the scope is in
                             // the "right" place for the parameter
    return a_saveLiveVariables | td->enter(scope, flags);

  check_aborted:
    if (__builtin_expect(scope->getAborted(), false))
        goto aborted;
    goto enter;

  aborted:
    return a_abortTransaction;
}

#endif // FAKE_ITM_BEGIN_TRANSACTION
