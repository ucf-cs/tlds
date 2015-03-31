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
 *  This file implements a simple bit filter datatype, with SSE2 optimizations.
 *  The type is templated by size, but has only been tested at a size of 1024
 *  bits.
 */

#ifndef BITFILTER_HPP__
#define BITFILTER_HPP__

#include <stm/config.h>
#include <stdint.h>

#if defined(STM_USE_SSE)
#include <xmmintrin.h>
#define FILTER_ALLOC(x) _mm_malloc((x), 16)
#else
#define FILTER_ALLOC(x) malloc((x))
#endif

namespace stm
{
  /**
   *  This is a simple Bit vector class, with SSE2 optimizations
   */
  template <uint32_t BITS>
  class BitFilter
  {
      /*** CONSTS TO ALLOW ACCESS VIA WORDS/SSE REGISTERS */
#ifdef STM_USE_SSE
      static const uint32_t VEC_SIZE    = 8 * sizeof(__m128i);
      static const uint32_t VEC_BLOCKS  = BITS / VEC_SIZE;
#endif
      static const uint32_t WORD_SIZE   = 8 * sizeof(uintptr_t);
      static const uint32_t WORD_BLOCKS = BITS / WORD_SIZE;

      /**
       *  index this as an array of words or an array of vectors
       */
      union {
#ifdef STM_USE_SSE
          mutable __m128i vec_filter[VEC_BLOCKS];
#endif
          uintptr_t word_filter[WORD_BLOCKS];
      } TM_ALIGN(16);

      /*** simple hash function for now */
      ALWAYS_INLINE
      static uint32_t hash(const void* const key)
      {
          return (((uintptr_t)key) >> 3) % BITS;
      }

    public:

      /*** constructor just clears the filter */
      BitFilter() { clear(); }

      /*** simple bit set function */
      TM_INLINE
      void add(const void* const val) volatile
      {
          const uint32_t index  = hash(val);
          const uint32_t block  = index / WORD_SIZE;
          const uint32_t offset = index % WORD_SIZE;
          word_filter[block] |= (1u << offset);
      }

      /*** simple bit set function, with strong ordering guarantees */
      ALWAYS_INLINE
      void atomic_add(const void* const val) volatile
      {
          const uint32_t index  = hash(val);
          const uint32_t block  = index / WORD_SIZE;
          const uint32_t offset = index % WORD_SIZE;
#if defined(STM_CPU_X86)
          atomicswapptr(&word_filter[block],
                        word_filter[block] | (1u << offset));
#else
          word_filter[block] |= (1u << offset);
          WBR;
#endif
      }

      /*** simple lookup */
      ALWAYS_INLINE
      bool lookup(const void* const val) const volatile
      {
          const uint32_t index  = hash(val);
          const uint32_t block  = index / WORD_SIZE;
          const uint32_t offset = index % WORD_SIZE;

          return word_filter[block] & (1u << offset);
      }

      /*** simple union */
      TM_INLINE
      void unionwith(const BitFilter<BITS>& rhs)
      {
#ifdef STM_USE_SSE
          for (uint32_t i = 0; i < VEC_BLOCKS; ++i)
              vec_filter[i] = _mm_or_si128(vec_filter[i], rhs.vec_filter[i]);
#else
          for (uint32_t i = 0; i < WORD_BLOCKS; ++i)
              word_filter[i] |= rhs.word_filter[i];
#endif
      }

      /*** a fast clearing function */
      TM_INLINE
      void clear() volatile
      {
#ifdef STM_USE_SSE
          // This loop gets automatically unrolled for BITS = 1024 by gcc-4.3.3
          const __m128i zero = _mm_setzero_si128();
          for (uint32_t i = 0; i < VEC_BLOCKS; ++i)
              vec_filter[i] = zero;
#else
          for (uint32_t i = 0; i < WORD_BLOCKS; ++i)
              word_filter[i] = 0;
#endif
      }

      /*** a bitwise copy method */
      TM_INLINE
      void fastcopy(const volatile BitFilter<BITS>* rhs) volatile
      {
#ifdef STM_USE_SSE
          for (uint32_t i = 0; i < VEC_BLOCKS; ++i)
              vec_filter[i] = const_cast<BitFilter<BITS>*>(rhs)->vec_filter[i];
#else
          for (uint32_t i = 0; i < WORD_BLOCKS; ++i)
              word_filter[i] = rhs->word_filter[i];
#endif
      }

      /*** intersect two vectors */
      NOINLINE bool intersect(const BitFilter<BITS>* rhs) const volatile
      {
#ifdef STM_USE_SSE
          // There is no clean way to compare an __m128i to zero, so we have
          // to union it with an array of uint64_ts, and then we can look at
          // the vector 64 bits at a time
          union {
              __m128i v;
              uint64_t i[2];
          } tmp;
          tmp.v = _mm_setzero_si128();
          for (uint32_t i = 0; i < VEC_BLOCKS; ++i) {
              __m128i intersect =
                  _mm_and_si128(const_cast<BitFilter<BITS>*>(this)->
                                vec_filter[i],
                                rhs->vec_filter[i]);
              tmp.v = _mm_or_si128(tmp.v, intersect);
          }

          return tmp.i[0]|tmp.i[1];
#else
          for (uint32_t i = 0; i < WORD_BLOCKS; ++i)
              if (word_filter[i] & rhs->word_filter[i])
                  return true;
          return false;
#endif
      }
  }; // class stm::BitFilter

}  // namespace stm

#endif // BITFILTER_HPP__
