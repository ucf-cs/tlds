/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

// This file implements sections 5.1 and 5.5 from the spec. They're together
// because they both want to deal directly with the thread-local descriptor that
// we are allocating statically here.


#include "libitm.h"
#include "Transaction.h"
#include "Scope.h"
#include "StackProtection.h"
#include "common/ThreadLocal.hpp"
#include "stm/txthread.hpp"
#include "stm/lib_globals.hpp"
using namespace stm;
using namespace itm2stm;

namespace {
/// Our thread local transaction descriptor. On most platforms this uses
/// __thread or its equivalent, but on Mac OS X---or if explicitly selected by
/// the user---it will use a specialized template-based Pthreads
/// implementation.
THREAD_LOCAL_DECL_TYPE(_ITM_transaction*) td;

/// This is what the stm library will call when it detects a conflict and needs
/// to abort. We always retry in this case, and if we have a registered thrown
/// object we ignore it (the thrown object only pertains to explicit
/// cancel-and-throw calls, which must happen in a consistent context).
///
/// Don't need any of the funky user-visible abort handling because the abort is
/// invisible. Just treat it like a restart of the current scope. This is passed
/// to sys_init.
///
/// Since the stm abort path doesn't protect the stack (RSTM assumes for the
/// moment that aborts are implemented via a longjmp-style mechanism), the best
/// that we can do is to protect the stack from here on. This is fine because we
/// /do/ have a longjmp-style abort mechanism, and any stack clobbering that
/// happens here is not an issue.
void
tmabort(stm::TxThread* tx) {
    // Clear the exception object if there is one. This is because tmabort is
    // called due to conflict aborts, and the exception isn't going to reach the
    // boundary. If we leave it in the scope, the rollback will filter out
    // rollback behavior for it, which we don't want.
    Scope* scope = static_cast<Scope*>(tx->scope);
    scope->clearThrownObject();
    scope->getOwner().restart(COMPUTE_PROTECTED_STACK_ADDRESS_DEFAULT(1));
}
}

// 5.1  Initialization and finalization functions

int
_ITM_initializeProcess(void) {
    if (td == NULL)
        sys_init(tmabort);
    return _ITM_initializeThread();
}

int
_ITM_initializeThread(void) {
    return !(_ITM_getTransaction() == NULL);
}

void
_ITM_finalizeThread(void) {
    if (td)
        delete td;
    td = NULL;
}

void
_ITM_finalizeProcess(void) {
    _ITM_finalizeThread();
    sys_shutdown();
}

// 5.5  State manipulation functions

_ITM_transaction*
_ITM_getTransaction(void) {
    if (!td) {
        TxThread::thread_init();
        td = new _ITM_transaction(*stm::Self);
    }
    return td;
}

_ITM_transactionId_t
_ITM_getTransactionId(_ITM_transaction* td) {
    return td->inner()->getId();
}

