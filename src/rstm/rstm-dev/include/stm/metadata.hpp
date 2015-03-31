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
 *  The global metadata types used by our STM implementations are defined in
 *  this file, along with many of the types used by the TxThread object of
 *  logging the progress of a transaction.
 */

#ifndef METADATA_HPP__
#define METADATA_HPP__

#include <stm/config.h>
#include "stm/MiniVector.hpp"
#include "stm/BitFilter.hpp"

namespace stm
{
  // forward declare for avoiding a SunCC issue
  NORETURN void UNRECOVERABLE(const char*);

  /**
   *  Many of our data structures benefit from having a cap on the number of
   *  threads.  Here we set that cap at 256
   */
  static const unsigned MAX_THREADS = 256;

  /**
   *  Forward declare the TxThread type, so we can use it in some of our
   *  metadata types
   */
  struct TxThread;

  /**
   *  A scope_t is an opaque type used by an API to unwind.
   */
  typedef void scope_t;

  /**
   *  id_version_t uses the msb as the lock bit.  If the msb is zero, treat
   *  the word as a version number.  Otherwise, treat it as a struct with the
   *  lower 8 bits giving the ID of the lock-holding thread.
   */
  union id_version_t
  {
      struct
      {
          // ensure msb is lock bit regardless of platform
#if defined(STM_CPU_X86) /* little endian */
          uintptr_t id:(8*sizeof(uintptr_t))-1;
          uintptr_t lock:1;
#else /* big endian (probably SPARC) */
          uintptr_t lock:1;
          uintptr_t id:(8*sizeof(uintptr_t))-1;
#endif
      } fields;
      uintptr_t all; // read entire struct in a single load
  };

  /**
   * When we acquire an orec, we may ultimately need to reset it to its old
   * value (if we abort).  Saving the old value with the orec is an easy way to
   * support this need without having exta logging in the descriptor.
   */
  struct orec_t
  {
      volatile id_version_t v; // current version number or lockBit + ownerId
      volatile uintptr_t    p; // previous version number
  };

  /**
   *  Nano requires that we log not just the orec address, but also its value
   */
  struct nanorec_t
  {
      orec_t* o;   // address of the orec
      uintptr_t v; // value of the orec
      nanorec_t(orec_t* _o, uintptr_t _v) : o(_o), v(_v) { }
  };

  /**
   *  TLRW-style algorithms don't use orecs, but instead use "byte locks".
   *  This is the type of a byte lock.  We have 32 bits for the lock, and
   *  then 60 bytes corresponding to 60 named threads.
   *
   *  NB: We don't support more than 60 threads in ByteLock-based algorithms.
   *      If you have more than that many threads, you should use adaptivity
   *      to switch to a different algorithm.
   */
  struct bytelock_t
  {
      volatile uint32_t      owner;      // no need for more than 32 bits
      volatile unsigned char reader[CACHELINE_BYTES - sizeof(uint32_t)];

      /**
       *  Setting the read byte is platform-specific, so we make it a method
       *  of the bytelock_t
       *
       *  NB: implemented in algs.hpp, so that it is visible where needed,
       *      but not visible globally
       */
      void set_read_byte(uint32_t id);
  };

  /**
   *  Padded word-sized value for keeping a value in its own cache line
   */
  struct pad_word_t
  {
      volatile uintptr_t val;
      char pad[CACHELINE_BYTES-sizeof(uintptr_t)];
  };

  /**
   * a reader record (rrec) holds bits representing up to MAX_THREADS reader
   * transactions
   *
   *  NB: methods are implemented in algs.hpp, so that they are visible where
   *      needed, but not visible globally
   */
  struct rrec_t
  {
      /*** MAX_THREADS bits, to represent MAX_THREADS readers */
      static const uint32_t BUCKETS = MAX_THREADS / (8*sizeof(uintptr_t));
      static const uint32_t BITS    = 8*sizeof(uintptr_t);
      volatile uintptr_t    bits[BUCKETS];

      /*** set a bit */
      void setbit(unsigned slot);

      /*** test a bit */
      bool getbit(unsigned slot);

      /*** unset a bit */
      void unsetbit(unsigned slot);

      /*** combine test and set */
      bool setif(unsigned slot);

      /*** bitwise or */
      void operator |= (rrec_t& rhs);
  };

  /**
   *  If we want to do an STM with RSTM-style visible readers, this lets us
   *  have an owner and a bunch of readers in a single struct, instead of via
   *  separate orec and rrec tables.  Note that these data structures do not
   *  have nice alignment
   */
  struct bitlock_t
  {
      volatile uintptr_t owner;    // this is the single wrter
      rrec_t             readers;  // large bitmap for readers
  };

  /**
   *  In order to avoid a circular dependency, we need to declare some
   *  WriteSet support here.
   */
  class WordLoggingWriteSetEntry;
  class ByteLoggingWriteSetEntry;
#if defined(STM_WS_WORDLOG)
  typedef WordLoggingWriteSetEntry WriteSetEntry;
#elif defined(STM_WS_BYTELOG)
  typedef ByteLoggingWriteSetEntry WriteSetEntry;
#else
#   error WriteSet logging granularity configuration error.
#endif

  /**
   *  Common TypeDefs
   */
  typedef MiniVector<orec_t*>      OrecList;     // vector of orecs
  typedef MiniVector<rrec_t*>      RRecList;     // vector of rrecs
  typedef MiniVector<bytelock_t*>  ByteLockList; // vector of bytelocks
  typedef MiniVector<bitlock_t*>   BitLockList;  // vector of bitlocks
  typedef BitFilter<1024>          filter_t;     // flat 1024-bit Bloom filter
  typedef MiniVector<nanorec_t>    NanorecList;  // <orec,val> pairs
  typedef MiniVector<void*>        AddressList;  // for the mmpolicy

  /**
   *  These are for counting consecutive aborts in a histogram.  We use them
   *  for measuring toxic transactions.  Note that there is special support
   *  for counting how many times an hourglass transaction commits or aborts.
   */
  struct toxic_histogram_t
  {
      /*** the highest number of consec aborts > 16 */
      uint32_t max;

      /*** how many hourglass commits occurred? */
      uint32_t hg_commits;

      /*** how many hourglass aborts occurred? */
      uint32_t hg_aborts;

      /*** histogram with 0-16 + overflow */
      uint32_t buckets[18];

      /*** on commit, update the appropriate bucket */
      void onCommit(uint32_t aborts);

      /*** simple printout */
      void dump();

      /*** on hourglass commit */
      void onHGCommit();

      /*** on hourglass abort() */
      void onHGAbort();

      /*** simple constructor */
      toxic_histogram_t() : max(0), hg_commits(0), hg_aborts(0)
      {
          for (int i = 0; i < 18; ++i)
              buckets[i] = 0;
      }
  };

  /**
   *  When STM_COUNTCONSEC_YES is not set, we don't do anything for these
   *  events
   */
  struct toxic_nop_t
  {
      void onCommit(uint32_t) { }
      void dump()             { }
      void onHGCommit()       { }
      void onHGAbort()        { }
  };

#ifdef STM_COUNTCONSEC_YES
  typedef toxic_histogram_t toxic_t;
#else
  typedef toxic_nop_t toxic_t;
#endif

} // namespace stm

#endif // METADATA_HPP__
