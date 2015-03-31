/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/**
 *  Ticket Implementation
 *
 *    This STM uses a single ticket lock for all concurrency control.  There is
 *    no parallelism, but it is very fair.
 */

#include "../profiling.hpp"
#include "algs.hpp"
#include <stm/UndoLog.hpp> // STM_DO_MASKED_WRITE

using stm::UNRECOVERABLE;
using stm::TxThread;
using stm::ticketlock;


/**
 *  Declare the functions that we're going to implement, so that we can avoid
 *  circular dependencies.
 */
namespace {
  struct Ticket
  {
      static TM_FASTCALL bool begin(TxThread*);
      static TM_FASTCALL void* read(STM_READ_SIG(,,));
      static TM_FASTCALL void write(STM_WRITE_SIG(,,,));
      static TM_FASTCALL void commit(STM_COMMIT_SIG(,));

      static stm::scope_t* rollback(STM_ROLLBACK_SIG(,,,));
      static bool irrevoc(STM_IRREVOC_SIG(,));
      static void onSwitchTo();
  };

  /**
   *  Ticket begin:
   */
  bool
  Ticket::begin(TxThread* tx) {
      // get the ticket lock
      tx->begin_wait = ticket_acquire(&ticketlock);
      tx->allocator.onTxBegin();
      return true;
  }

  /**
   *  Ticket commit:
   */
  void
  Ticket::commit(STM_COMMIT_SIG(tx,)) {
      // release the lock, finalize mm ops, and log the commit
      ticket_release(&ticketlock);
      OnCGLCommit(tx);
  }

  /**
   *  Ticket read
   */
  void*
  Ticket::read(STM_READ_SIG(,addr,)) {
      return *addr;
  }

  /**
   *  Ticket write
   */
  void
  Ticket::write(STM_WRITE_SIG(,addr,val,mask)) {
      STM_DO_MASKED_WRITE(addr, val, mask);
  }

  /**
   *  Ticket unwinder:
   *
   *    In Ticket, aborts are never valid
   */
  stm::scope_t*
  Ticket::rollback(STM_ROLLBACK_SIG(,,,))
  {
      UNRECOVERABLE("ATTEMPTING TO ABORT AN IRREVOCABLE TICKET TRANSACTION");
      return NULL;
  }

  /**
   *  Ticket in-flight irrevocability:
   *
   *    Since we're already irrevocable, this code should never get called.
   *    Instead, the become_irrevoc() call should just return true.
   */
  bool
  Ticket::irrevoc(STM_IRREVOC_SIG(,))
  {
      UNRECOVERABLE("IRREVOC_TICKET SHOULD NEVER BE CALLED");
      return false;
  }

  /**
   *  Switch to Ticket:
   *
   *    For now, no other algs use the ticketlock variable, so no work is needed
   *    in this function.
   */
  void
  Ticket::onSwitchTo() {
  }
}

namespace stm {
  /**
   *  Ticket initialization
   */
  template<>
  void initTM<Ticket>()
  {
      // set the name
      stms[Ticket].name      = "Ticket";

      // set the pointers
      stms[Ticket].begin     = ::Ticket::begin;
      stms[Ticket].commit    = ::Ticket::commit;
      stms[Ticket].read      = ::Ticket::read;
      stms[Ticket].write     = ::Ticket::write;
      stms[Ticket].rollback  = ::Ticket::rollback;
      stms[Ticket].irrevoc   = ::Ticket::irrevoc;
      stms[Ticket].switcher  = ::Ticket::onSwitchTo;
      stms[Ticket].privatization_safe = true;
  }
}
