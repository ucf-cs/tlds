/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef STM_VALUE_LIST_HPP
#define STM_VALUE_LIST_HPP

/**
 *  We use the ValueList class to log address/value pairs for our
 *  value-based-validation implementations---NOrec and NOrecPrio currently. We
 *  generally log things at word granularity, and during validation we check to
 *  see if any of the bits in the word has changed since the word was originally
 *  read. If they have, then we have a conflict.
 *
 *  This word-granularity continues to be correct when we have enabled byte
 *  logging (because we're building for C++ TM compatibility), but it introduces
 *  the possibility of byte-level false conflicts. One of VBV's advantages is
 *  that there are no false conflicts. In order to preserve this behavior, we
 *  offer the user the option to use the byte-mask (which is already enabled for
 *  byte logging) to do byte-granularity validation. The disadvantage to this
 *  technique is that the read log entry size is increased by the size of the
 *  stored mask (we could optimize for 64-bit Linux and pack the mask into an
 *  unused part of the logged address, but we don't yet have this capability).
 *
 *  This file implements the value log given the current configuration settings
 *  in stm/config.h
 */
#include "stm/config.h"
#include "stm/MiniVector.hpp"

namespace stm {
  /**
   *  When we're word logging we simply store address/value pairs in the
   *  ValueList.
   */
  class WordLoggingValueListEntry {
      void** addr;
      void* val;

    public:
      WordLoggingValueListEntry(void** a, void* v) : addr(a), val(v) {
      }

      /**
       *  When word logging, we can just check if the address still has the
       *  value that we read earlier.
       */
      bool isValid() const {
          return *addr == val;
      }
  };

  /**
   *  When we're byte-logging we store a third word, the mask, and use it in the
   *  isValid() operation. The value we store is stored in masked form, which is
   *  an extra operation of overhead for single-threaded execution, but saves us
   *  masking during validation.
   */
  class ByteLoggingValueListEntry {
      void** addr;
      void* val;
      uintptr_t mask;

    public:
      ByteLoggingValueListEntry(void** a, void* v, uintptr_t m)
          : addr(a), val(v), mask(m) {
      }

      /**
       *  When we're dealing with byte-granularity we need to check values on a
       *  per-byte basis.
       *
       *  We believe that this implementation is safe because the logged address
       *  is *always* word aligned, thus promoting subword loads to aligned word
       *  loads followed by a masking operation will not cause any undesired HW
       *  behavior (page fault, etc.).
       *
       *  We're also assuming that the masking operation means that any
       *  potential "low-level" race that we introduce is immaterial---this may
       *  or may not be safe in C++1X. As an example, someone is
       *  nontransactionally writing the first byte of a word and we're
       *  transactionally reading the scond byte. There is no language-level
       *  race, however when we promote the transactional byte read to a word,
       *  we read the same location the nontransactional access is writing, and
       *  there is no intervening synchronization. We're safe from some bad
       *  behavior because of the atomicity of word-level accesses, and we mask
       *  out the first byte, which means the racing read was actually
       *  dead. There are no executions where the source program can observe the
       *  race and thus they conclude that it is race-free.
       *
       *  I don't know if this argument is valid, but it is certainly valid for
       *  now, since there is no memory model for C/C++.
       *
       *  If this becomes a problem we can switch to a loop-when-mask != ~0x0
       *  approach.
       */
      bool isValid() const {
          return ((uintptr_t)val & mask) == ((uintptr_t)*addr & mask);
      }
  };

#if defined(STM_WS_WORDLOG) || defined(STM_USE_WORD_LOGGING_VALUELIST)
  typedef WordLoggingValueListEntry ValueListEntry;
#define STM_VALUE_LIST_ENTRY(addr, val, mask) ValueListEntry(addr, val)
#elif defined(STM_WS_BYTELOG)
  typedef ByteLoggingValueListEntry ValueListEntry;
#define STM_VALUE_LIST_ENTRY(addr, val, mask) ValueListEntry(addr, val, mask)
#else
#error "Preprocessor configuration error: STM_WS_(WORD|BYTE)LOG should be set"
#endif

  struct ValueList : public MiniVector<ValueListEntry> {
      ValueList(const unsigned long cap) : MiniVector<ValueListEntry>(cap) {
      }
  };
}

#endif // STM_VALUE_LIST_HPP
