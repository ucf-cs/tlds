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
 *  The RSTM backends that use redo logs all rely on this datastructure,
 *  which provides O(1) clear, insert, and lookup by maintaining a hashed
 *  index into a vector.
 */

#ifndef WRITESET_HPP__
#define WRITESET_HPP__

#include <stm/config.h>

#ifdef STM_CC_SUN
#include <string.h>
#include <stdlib.h>
#else
#include <cstdlib>
#include <cstring>
#endif
#include <cassert>

namespace stm
{
  /**
   *  The WriteSet implementation is heavily influenced by the configuration
   *  parameters, STM_WS_(WORD/BYTE)LOG, STM_PROTECT_STACK, and
   *  STM_ABORT_ON_THROW. This means that much of this file is ifdeffed
   *  accordingly.
   */

  /**
   * The log entry type when we're word-logging is pretty trivial, and just
   * logs address/value pairs.
   */
  struct WordLoggingWriteSetEntry
  {
      void** addr;
      void*  val;

      WordLoggingWriteSetEntry(void** paddr, void* pval)
          : addr(paddr), val(pval)
      { }

      /**
       *  Called when we are WAW an address, and we want to coalesce the
       *  write. Trivial for the word-based writeset, but complicated for the
       *  byte-based version.
       */
      void update(const WordLoggingWriteSetEntry& rhs) { val = rhs.val; }

      /**
       * Check to see if the entry is completely contained within the given
       * address range. We have some preconditions here w.r.t. alignment and
       * size of the range. It has to be at least word aligned and word
       * sized. This is currently only used with stack addresses, so we don't
       * include asserts because we don't want to pay for them in the common
       * case writeback loop.
       */
      bool filter(void** lower, void** upper)
      {
          return !(addr + 1 < lower || addr >= upper);
      }

      /**
       * Called during writeback to actually perform the logged write. This is
       * trivial for the word-based set, but the byte-based set is more
       * complicated.
       */
      void writeback() const { *addr = val; }

      /**
       * Called during rollback if there is an exception object that we need to
       * perform writes to. The address range is the range of addresses that
       * we're looking for. If this log entry is contained in the range, we
       * perform the writeback.
       *
       * NB: We're assuming a pretty well defined address range, in terms of
       *     size and alignment here, because the word-based writeset can only
       *     handle word-sized data.
       */
      void rollback(void** lower, void** upper)
      {
          assert((uint8_t*)upper - (uint8_t*)lower >= (int)sizeof(void*));
          assert((uintptr_t)upper % sizeof(void*) == 0);
          if (addr >= lower && (addr + 1) <= upper)
              writeback();
      }
  };

  /**
   * The log entry for byte logging is complicated by
   *
   *   1) the fact that we store a bitmask
   *   2) that we need to treat the address/value/mask instance variables as
   *      both word types, and byte types.
   *
   *  We do this with unions, which makes the use of these easier since it
   *  reduces the huge number of casts we perform otherwise.
   *
   *  Union naming is important, since the outside world only directly deals
   *  with the word-sized fields.
   */
  struct ByteLoggingWriteSetEntry
  {
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
          uint8_t   byte_mask[sizeof(void*)];
      };

      ByteLoggingWriteSetEntry(void** paddr, void* pval, uintptr_t pmask)
      {
          addr = paddr;
          val  = pval;
          mask = pmask;
      }

      /**
       *  Called when we are WAW an address, and we want to coalesce the
       *  write. Trivial for the word-based writeset, but complicated for the
       *  byte-based version.
       *
       *  The new value is the bytes from the incoming log injected into the
       *  existing value, we mask out the bytes we want from the incoming word,
       *  mask the existing word, and union them.
       */
      void update(const ByteLoggingWriteSetEntry& rhs)
      {
          // fastpath for full replacement
          if (__builtin_expect(rhs.mask == (uintptr_t)~0x0, true)) {
              val = rhs.val;
              mask = rhs.mask;
          }

          // bit twiddling for awkward intersection, avoids looping
          uintptr_t new_val = (uintptr_t)rhs.val;
          new_val &= rhs.mask;
          new_val |= (uintptr_t)val & ~rhs.mask;
          val = (void*)new_val;

          // the new mask is the union of the old mask and the new mask
          mask |= rhs.mask;
      }

      /**
       *  Check to see if the entry is completely contained within the given
       *  address range. We have some preconditions here w.r.t. alignment and
       *  size of the range. It has to be at least word aligned and word
       *  sized. This is currently only used with stack addresses, so we
       *  don't include asserts because we don't want to pay for them in the
       *  common case writeback loop.
       *
       *  The byte-logging writeset can actually accommodate awkward
       *  intersections here using the mask, but we're not going to worry
       *  about that given the expected size/alignment of the range.
       */
      bool filter(void** lower, void** upper)
      {
          return !(addr + 1 < lower || addr >= upper);
      }

      /**
       *  If we're byte-logging, we'll write out each byte individually when
       *  we're not writing a whole word. This turns all subword writes into
       *  byte writes, so we lose the original atomicity of (say) half-word
       *  writes in the original source. This isn't a correctness problem
       *  because of our transactional synchronization, but could be a
       *  performance problem if the system depends on sub-word writes for
       *  performance.
       */
      void writeback() const
      {
          if (__builtin_expect(mask == (uintptr_t)~0x0, true)) {
              *addr = val;
              return;
          }

          // mask could be empty if we filtered out all of the bytes
          if (mask == 0x0)
              return;

          // write each byte if its mask is set
          for (unsigned i = 0; i < sizeof(val); ++i)
              if (byte_mask[i] == 0xff)
                  byte_addr[i] = byte_val[i];
      }

      /**
       *  Called during the rollback loop in order to write out buffered
       *  writes to an exception object (represented by the address
       *  range). We don't assume anything about the alignment or size of the
       *  exception object.
       */
      void rollback(void** lower, void** upper)
      {
          // two simple cases first, no intersection or complete intersection.
          if (addr + 1 < lower || addr >= upper)
              return;

          if (addr >= lower && addr + 1 <= upper) {
              writeback();
              return;
          }

          // odd intersection
          for (unsigned i = 0; i < sizeof(void*); ++i) {
              if ((byte_mask[i] == 0xff) &&
                  (byte_addr + i >= (uint8_t*)lower ||
                   byte_addr + i < (uint8_t*)upper))
                  byte_addr[i] = byte_val[i];
          }
      }
  };

  /**
   *  Pick a write-set implementation, based on the configuration.
   */
#if defined(STM_WS_WORDLOG)
  typedef WordLoggingWriteSetEntry WriteSetEntry;
#   define STM_WRITE_SET_ENTRY(addr, val, mask) addr, val
#elif defined(STM_WS_BYTELOG)
  typedef ByteLoggingWriteSetEntry WriteSetEntry;
#   define STM_WRITE_SET_ENTRY(addr, val, mask) addr, val, mask
#else
#   error WriteSet logging granularity configuration error.
#endif

  /**
   *  The write set is an indexed array of WriteSetEntry elements.  As with
   *  MiniVector, we make sure that certain expensive but rare functions are
   *  never inlined.
   */
  class WriteSet
  {
      /***  data type for the index */
      struct index_t
      {
          size_t version;
          void*  address;
          size_t index;

          index_t() : version(0), address(NULL), index(0) { }
      };

      index_t* index;                             // hash entries
      size_t   shift;                             // for the hash function
      size_t   ilength;                           // max size of hash
      size_t   version;                           // version for fast clearing

      WriteSetEntry* list;                        // the array of actual data
      size_t   capacity;                          // max array size
      size_t   lsize;                             // elements in the array


      /**
       *  hash function is straight from CLRS (that's where the magic
       *  constant comes from).
       */
      size_t hash(void* const key) const
      {
          static const unsigned long long s = 2654435769ull;
          const unsigned long long r = ((unsigned long long)key) * s;
          return (size_t)((r & 0xFFFFFFFF) >> shift);
      }

      /**
       *  This doubles the size of the index. This *does not* do anything as
       *  far as actually doing memory allocation. Callers should delete[]
       *  the index table, increment the table size, and then reallocate it.
       */
      size_t doubleIndexLength();

      /**
       *  Supporting functions for resizing.  Note that these are never
       *  inlined.
       */
      void rebuild();
      void resize();
      void reset_internal();

    public:

      WriteSet(const size_t initial_capacity);
      ~WriteSet();

      /**
       *  Search function.  The log is an in/out parameter, and the bool
       *  tells if the search succeeded. When we are byte-logging, the log's
       *  mask is updated to reflect the bytes in the returned value that are
       *  valid. In the case that we don't find anything, the mask is set to 0.
       */
      bool find(WriteSetEntry& log) const
      {
          size_t h = hash(log.addr);

          while (index[h].version == version) {
              if (index[h].address != log.addr) {
                  // continue probing
                  h = (h + 1) % ilength;
                  continue;
              }
#if defined(STM_WS_WORDLOG)
              log.val = list[index[h].index].val;
              return true;
#elif defined(STM_WS_BYTELOG)
              // Need to intersect the mask to see if we really have a match. We
              // may have a full intersection, in which case we can return the
              // logged value. We can have no intersection, in which case we can
              // return false. We can also have an awkward intersection, where
              // we've written part of what we're trying to read. In that case,
              // the "correct" thing to do is to read the word from memory, log
              // it, and merge the returned value with the partially logged
              // bytes.
              WriteSetEntry& entry = list[index[h].index];
              if (__builtin_expect((log.mask & entry.mask) == 0, false)) {
                  log.mask = 0;
                  return false;
              }

              // The update to the mask transmits the information the caller
              // needs to know in order to distinguish between a complete and a
              // partial intersection.
              log.val = entry.val;
              log.mask = entry.mask;
              return true;
#else
#error "Preprocessor configuration error."
#endif
          }

#if defined(STM_WS_BYTELOG)
          log.mask = 0x0;
#endif
          return false;
      }

      /**
       *  Support for abort-on-throw and stack protection makes rollback
       *  tricky.  We might need to write to an exception object, and/or
       *  filter writeback to protect the stack.
       *
       *  NB: We use a macro to hide the fact that some rollback calls are
       *      really simple.  This gets called by ~30 STM implementations
       */
#if !defined (STM_ABORT_ON_THROW)
      void rollback() { }
#   define STM_ROLLBACK(log, stack, exception, len) log.rollback()
#else
#   if !defined(STM_PROTECT_STACK)
      void rollback(void**, size_t);
#   define STM_ROLLBACK(log, stack, exception, len) log.rollback(exception, len)
#   else
      void rollback(void**, void**, size_t);
#   define STM_ROLLBACK(log, stack, exception, len) log.rollback(stack, exception, len)
#   endif
#endif

      /**
       *  Encapsulate writeback in this routine, so that we can avoid making
       *  modifications to lots of STMs when we need to change writeback for a
       *  particular compiler.
       */
#if !defined(STM_PROTECT_STACK)
      TM_INLINE void writeback()
      {
#else
      TM_INLINE void writeback(void** upper_stack_bound)
      {
#endif
          for (iterator i = begin(), e = end(); i != e; ++i)
          {
#ifdef STM_PROTECT_STACK
              // See if this falls into the protected stack region, and avoid
              // the writeback if that is the case. The filter call will update
              // a byte-log's mask if there is an awkward intersection.
              //
              void* top_of_stack;
              if (i->filter(&top_of_stack, upper_stack_bound))
                  continue;
#endif
              i->writeback();
          }
      }

      /**
       *  Inserts an entry in the write set.  Coalesces writes, which can
       *  appear as write reordering in a data-racy program.
       */
      void insert(const WriteSetEntry& log)
      {
          size_t h = hash(log.addr);

          //  Find the slot that this address should hash to. If we find it,
          //  update the value. If we find an unused slot then it's a new
          //  insertion.
          while (index[h].version == version) {
              if (index[h].address != log.addr) {
                  h = (h + 1) % ilength;
                  continue; // continue probing at new h
              }

              // there /is/ an existing entry for this word, we'll be updating
              // it no matter what at this point
              list[index[h].index].update(log);
              return;
          }

          // add the log to the list (guaranteed to have space)
          list[lsize] = log;

          // update the index
          index[h].address = log.addr;
          index[h].version = version;
          index[h].index   = lsize;

          // update the end of the list
          lsize += 1;

          // resize the list if needed
          if (__builtin_expect(lsize == capacity, false))
              resize();

          // if we reach our load-factor
          // NB: load factor could be better handled rather than the magic
          //     constant 3 (used in constructor too).
          if (__builtin_expect((lsize * 3) >= ilength, false))
              rebuild();
      }

      /*** size() lets us know if the transaction is read-only */
      size_t size() const { return lsize; }

      /**
       *  We use the version number to reset in O(1) time in the common case
       */
      void reset()
      {
          lsize    = 0;
          version += 1;

          // check overflow
          if (version != 0)
              return;
          reset_internal();
      }

      /*** Iterator interface: iterate over the list, not the index */
      typedef WriteSetEntry* iterator;
      iterator begin() const { return list; }
      iterator end()   const { return list + lsize; }
  };
}

#endif // WRITESET_HPP__
