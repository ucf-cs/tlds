/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_ITM2STM_TRANSACTION_H
#define STM_ITM2STM_TRANSACTION_H

#include "libitm.h"
#include "Scope.h"

namespace stm {
struct TxThread;
} // namespace stm

/// This needs to be in the global namespace because that's how it's declared in
/// Intel's libitm.h header. We forward many of the ITM ABI calls to inlined
/// _ITM_transaction member functions. We treat the instance variables as
/// private, even though we can't use a private modifier because of the way
/// _ITM_transaction is declared.
///
/// The implementation of these functions is distributed throughout the
/// Transaction.cpp and libitm-*.cpp files, so that we don't have to have all of
/// the inline implementations in this header---most of them are only used in
/// the single libitm file that is relevent to them. Anything that we need
/// inlined in more than one source file is defined here.
///
/// We use gcc's "fastcall" calling convention for those things that we don't
/// inline here but are used across multiple source files (there aren't that
/// many of them).
struct _ITM_transaction {

    /// Right now we manage scopes as a linked-list stack implementation. There
    /// are obviously alternatives to this, like a vector (or stm::MiniVector),
    /// but this seemed easiest for our initial implementation.
    ///
    /// We use a sort of "intrusive" list node by inheriting from the Scope type
    /// and adding the "next" pointer. We can do this because this isn't a
    /// generic list solution, so we know what the Scope constructor looks like.
    struct Node : public itm2stm::Scope {
        Node* next_;
        Node(_ITM_transaction&);
        ~Node();
    };

    stm::TxThread&  thread_handle_; // the pointer to the stm library descriptor
    Node*             outer_scope_; // we need fast access to the outer scope
    Node*                  scopes_; // the scope stack
    Node*             free_scopes_; // a freelist for scope nodes
    _ITM_transactionId_t next_tid_; // implements the ABI unique id requirement

    // The _ITM_abortReason is an enum that doesn't have an enumeration for
    // 0. We want to be able to detect that there wasn't a previous abort
    // reason, but there's no _ITM_abortReason we can use for that. This union
    // allows us to use 0 (false) to indicate that there was no previous abort,
    // without having type problems (0 is not a valie _ITM_abortReason). This
    // saves us a word of space, and shouldn't be too complicated to maintain
    // doing forwards.
    union {
        _ITM_abortReason prev_abort_reason_;
        int32_t                 prev_abort_;
    };

    /// Constructor needs a reference to the stm library descriptor for this
    /// thread.
    _ITM_transaction(stm::TxThread&);
    ~_ITM_transaction();


    /// The innermost active scope. This is primarily used internally, and is
    /// used so that we can change the stack implementation as painlessly as
    /// possible.
    inline Node* inner() const {
        return scopes_;
    }

    /// The thread handle for RSTM. Used frequently in the various barrier
    /// (read/write/log) code.
    inline stm::TxThread& handle() const {
        return thread_handle_;
    }

    /// Perform whatever actions are required to undo the effects of the inner
    /// scope. Supports all of the various ways a transaction can abort (cancel,
    /// retry, conflict abort). We need a stack address that represents the
    /// lower bound of the protected stack region, so that an undo-log TM
    /// doesn't clobber any part of the stack that we're going to need before we
    /// \code{restart} or \code{cancel} the scope.
    void rollback(void** protected_stack_lower_bound);

    /// Performs whatever actions are required to commit a scope. Unlike
    /// rollback, the caller doesn't need to leave the scope---it's done
    /// internally. We need a stack address that represents the lower bound of
    /// the protected stack region so that a redo-log TM doesn't clobber any
    /// part of the stack that we need to complete the ABI call.
    void commit(void** protected_stack_lower_bound);

    /// Performs whatever actions are required to commit a scope, but returning
    /// "false" if the commit fails rather than doing a non-local control
    /// transfer. Like "commit" the caller does not need to leave the scope. We
    /// need a stack address that represents the lower bound of the protected
    /// stack region so that a redo-log TM doesn't clobber any part of the stack
    /// that we need.
    bool tryCommit(void** protected_stack_lower_bound);

    /// This is a bit of a confusing call as described in the ABI. It seems to
    /// mean that we're supposed to just pop scopes, without rolling back or
    /// checking for conflicts, until we find the correct transaction ID. We're
    /// not sure if we need to protect the stack here.
    void commitToId(_ITM_transactionId_t);

    /// This is defined for the make "fake_begin" target, and generates an asm
    /// example that helps us implement the _ITM_beginTransaction asm file.
#ifdef FAKE_ITM_BEGIN_TRANSACTION

    /// Used exclusively in the _ITM_beginTransaction.S barrier to get a
    /// checkpoint (scope). Generally this will just pull a node off of the
    /// free list, but it may need to allocate a new scope.
    ///
    /// Note that this doesn't actually get called directly, it is just used so
    /// that we can get an idea of the code required to inline this (see
    /// itm2stm-5.7.cpp::_FAKE_beginTransaction).
    Node* getScope() __attribute__((always_inline));

#endif // FAKE_ITM_BEGIN_TRANSACTION

    /// Perform whatever actions are necessary to enter a scope (the scope's
    /// checkpoint has already been initialized before this call). Returns the
    /// requested behavior _ITM_action, either run instrumented or run
    /// uninstrumented---the caller needs to add the restoreLiveVariables or
    /// saveLiveVariables as necessary.
    ///
    /// We give it an explicit asm label so that we can use it in our
    /// _ITM_beginTransaction.S implementation without relying on a particular
    /// C++ name-mangling implementation.
    uint32_t enter(Node* const scope, const uint32_t flags)
        asm("_stm_itm2stm_transaction_enter") GCC_FASTCALL;

    /// Reentering a scope on restart is slightly different than entering a
    /// scope for the first time. This handles that difference.
    uint32_t reenter(Node* const scope);

    /// We need to be able to "new" nodes from asm in _ITM_beginTransaction, but
    /// we can't give a constructor an asm label (it appears to be an
    /// undocumented restriction), so we use this as a stand-in. The Node
    /// constructor takes an _ITM_transaction& so we use an _ITM_transaction
    /// member function to get the "this" pointer automatically. We could use a
    /// static with an explicit _ITM_transaction& parameter as well.
    ///
    /// As with enter we provide an asm label that is independent of the C++
    /// name-mangler---in this case the entire purpose of this function /is/
    /// this label.
    ///
    /// Defined in itm2stm-5.7.cpp
    Node* NewNode() asm("_stm_itm2stm_transaction_new_node");

    /// Leaves the inner scope by popping the scope off the scopes stack.
    /// Returns the popped scope so that it can be "restored" by cancel or
    /// restart. The scope should either be rolled back or committed before this
    /// call. Leave reclaims the scope (though it is still safe to use... it's
    /// just on the free list for the next call to getScope).
    ///
    /// This needs to be fast for the commit code, and it's also used in the
    /// abort path, so we implement it in libitm-5.9.cpp so it can get inlined
    /// there (we're not super-concerned about the abort performance).
    ///
    /// We use the GCC fastcall calling convention so that it's as efficient as
    /// possible in the abort path for x86.
    GCC_FASTCALL Node* leave() __attribute__((used));

    /// Corresponds to aborting a scope and continuing execution outside of the
    /// scope---this is the standard C++ cancel mechanism. This calls rollback
    /// internally, thus we need a stack address to serve as the protected stack
    /// lower bound.
    void cancel(void** protected_stack_lower_bound) NORETURN;

    /// Corresponds to aborting and retrying a scope. Implemented here by
    /// rolling back and leaving (popping) the scope, and then re-executing the
    /// enter functionality, which puts the scope back on the scopes stack. This
    /// calls rollback internally, thus we need a stack address to serve as the
    /// protected stack lower bound.
    ///
    /// Inlined in libitm-5.8, and marked as used for tmabort in libitm-5.1,5.
    void restart(void** protected_stack_lower_bound) NORETURN
        __attribute__((used));

    /// Implements the abort logic layed out in the ABI specification. Abort is
    /// implemented in terms of "rollback," "leave," "cancel," and "restart."
    /// This serves as one of the initial entry point to rollback and thus we
    /// need a stack address to serve as the protected stack lower bound.
    void abort(_ITM_abortReason why, void** protected_stack_lower_bound)
        NORETURN;

    /// Wraps the logic that checks if the library is irrevocable. Used in both
    /// 5.4 and 5.7, but implemented in 5.7 because we'd like to inine it there.
    GCC_FASTCALL bool libraryIsInevitable() const;

    /// These basically just forward to the correct scopes. Defined in
    /// itm2stm-5.17.cpp, where they are used.
    void registerOnAbort(_ITM_userUndoFunction, void* arg);
    void registerOnCommit(_ITM_userCommitFunction, _ITM_transactionId_t, void*
                          arg);
};

#endif // STM_ITM2STM_TRANSACTION_H
