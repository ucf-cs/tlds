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
 *  Implement an undo log so that we can centralize all logic for
 *  stack-filtering and abort/throw behavior in in-place update STMs
 */

#ifndef UNDO_LOG_HPP__
#define UNDO_LOG_HPP__

#include <stm/config.h>
#include "stm/MiniVector.hpp"
#include "stm/macros.hpp"

/**
 *  An undo log is a pretty simple structure. We never need to search it, so
 *  its only purpose is to store stuff and write stuff out when we
 *  abort. It's split out into its own class in order to deal with the
 *  configuration-based behavior that we need it to observe, like
 *  byte-accesses, stack protection, etc.
 */
namespace stm
{
  /**
   *  The undo log entry is the type stored in the undo log. If we're
   *  byte-logging then it has a mask, otherwise it's just an address-value
   *  pair.
   */

  /***  Word-logging variant of undo log entries */
  struct WordLoggingUndoLogEntry
  {
      void** addr;
      void*  val;

      WordLoggingUndoLogEntry(void** addr, void* val)
          : addr(addr), val(val)
      { }

      /***  for word logging, we undo an entry by simply writing it back */
      inline void undo() const { *addr = val; }

      /**
       *  Called in order to find out if the logged word falls within the
       *  address range. This is used to both filter out undo operations to
       *  the protected stack, and the exception object.
       *
       *  Note that while the wordlog can _only_ be completely filtered out,
       *  we don't have support for partial filtering here. This is almost
       *  certainly ok, since the stack is word aligned, and an exception
       *  object is probably aligned as well, and at least a word large.
       *
       *  The bytelog version /can/ be partially filtered, which is just a
       *  masking operation.
       */
      inline bool filter(void** lower, void** upper)
      {
          return (addr >= lower && addr + 1 < upper);
      }
  };

  /***  Byte-logging variant of undo log entries */
  struct ByteLoggingUndoLogEntry
  {
      // We use unions here to make our life easier, since we may access
      // these as byte, bytes, or word
      union {
          void**   addr;
          uint8_t* byte_addr;
      };

      union {
          void*   val;
          uint8_t byte_val[sizeof(void*)];
      };

      union {
          uintptr_t mask;
          uint8_t   byte_mask[sizeof(uintptr_t)];
      };

      /***  construction is simple, except for the unions */
      ByteLoggingUndoLogEntry(void** paddr, void* pval, uintptr_t pmask)
          : addr(), val(), mask()
      {
          addr = paddr;
          val  = pval;
          mask = pmask;
      }

      /*** write (undo) a log entry */
      inline static void DoMaskedWrite(void** addr, void* val, uintptr_t mask)
      {
          // common case is word access
          if (mask == ~(uintptr_t)0x0) {
              *addr = val;
              return;
          }

          // simple check for null mask, which might result from a filter call
          if (mask == 0x0)
              return;

          union {
              void**   word;
              uint8_t* bytes;
          } uaddr = { addr };

          union {
              void*   word;
              uint8_t bytes[sizeof(void*)];
          } uval = { val };

          union {
              uintptr_t word;
              uint8_t   bytes[sizeof(uintptr_t)];
          } umask = { mask };

          // We're just going to write out individual bytes, which turns all
          // subword accesses into byte accesses. This might be inefficient but
          // should be correct, since the locations that we're writing to are
          // supposed to be locked, and if there's a data race we can have any
          // sort of behavior.
          for (unsigned i = 0; i < sizeof(void*); ++i)
              if (umask.bytes[i] == 0xFF)
                  uaddr.bytes[i] = uval.bytes[i];
      }

      inline void undo() const { DoMaskedWrite(addr, val, mask); }

      /**
       *  The bytelog implementation of the filter operation support any sort
       *  of intersection possible.
       */
      inline bool filter(void** lower, void** upper)
      {
          // fastpath when there's no intersection
          return (addr + 1 < lower || addr >= upper)
              ? false
              : filterSlow(lower, upper);
      }

    private:

      /**
       *  We outline the slowpath filter. If this /ever/ happens it will be
       *  such a corner case that it just doesn't matter. Plus this is an
       *  abort path anyway.
       */
      bool filterSlow(void**, void**);
  };

  /**
   * Macros for hiding the WS_(WORD/BYTE)MASK API differences.
   */
#if defined(STM_WS_WORDLOG)
  typedef WordLoggingUndoLogEntry UndoLogEntry;
#define STM_UNDO_LOG_ENTRY(addr, val, mask) addr, val
#define STM_DO_MASKED_WRITE(addr, val, mask) *addr = val
#elif defined(STM_WS_BYTELOG)
  typedef ByteLoggingUndoLogEntry UndoLogEntry;
#define STM_UNDO_LOG_ENTRY(addr, val, mask) addr, val, mask
#define STM_DO_MASKED_WRITE(addr, val, mask) \
    stm::ByteLoggingUndoLogEntry::DoMaskedWrite(addr, val, mask)
#else
#   error Configuration logic error.
#endif

  class UndoLog : public stm::MiniVector<UndoLogEntry>
  {
    public:
      UndoLog(const uintptr_t cap) : stm::MiniVector<UndoLogEntry>(cap) { }

      /**
       * A utility for undo-log implementations that undoes all of the
       * accesses in a write log except for those that took place to an
       * exception object that is being thrown-on-abort. This is mainly to
       * support the itm2stm shim at the moment, since baseline stm doesn't
       * have abort-on-throw capabilities.
       */
#if !defined(STM_PROTECT_STACK) && !defined(STM_ABORT_ON_THROW)
      void undo();
#   define STM_UNDO(log, stack, except, len) log.undo()
#elif defined(STM_PROTECT_STACK) && !defined(STM_ABORT_ON_THROW)
      void undo(void** upper_stack_bound);
#   define STM_UNDO(log, stack, except, len) log.undo(stack)
#elif !defined(STM_PROTECT_STACK) && defined(STM_ABORT_ON_THROW)
      void undo(void** except, size_t len);
#   define STM_UNDO(log, stack, except, len) log.undo(except, len)
#elif defined(STM_PROTECT_STACK) && defined(STM_ABORT_ON_THROW)
      void undo(void** upper_stack_bound, void** exception, size_t len);
#   define STM_UNDO(log, stack, except, len) log.undo(stack, except, len)
#else
#   error if/else logic error
#endif
  };
}
#endif // UNDO_LOG_HPP__
