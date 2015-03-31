/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef API_LIBRARY_INST_HPP__
#define API_LIBRARY_INST_HPP__

#include <stm/config.h>

/**
 *  In the LIBRARY api, the transformation of reads and writes of addresses
 *  into correctly formed calls to the tmread and tmwrite functions is
 *  achieved through a set of templates.  The role of the templates is to
 *  allow a single library call to be transformed into the right instructions
 *  to read at any supported size/type, even though the library itself only
 *  provides word-level read/write functions.
 *
 *  This file presents those templates, to reduce clutter in the main
 *  library.hpp file.
 *
 *  This file should be included in the middle of the library file.  It has no
 *  includes of its own.
 *
 *  Also, BE WARNED: this implementation of the library API allows "granular
 *  lost updates".  If transaction A writes a single char, and thread B writes
 *  an adjacent char, then B's write could be lost.
 */

namespace stm
{
  /**
   *  The DISPATCH class takes an address and a type, and determines which
   *  words (represented as void*s) ought to be read and written to effect a
   *  read or write of the given type, from the given address.
   *
   *  NB: if the compiler can't find a valid specialization, it will use this
   *      variant, which will cause an error.  This is the desired behavior.
   */
  template <typename T, size_t S>
  struct DISPATCH
  {
      // use this to ensure compile-time errors
      struct InvalidTypeAsSecondTemplateParameter { };

      // the read method will transform a read to a sizeof(T) byte range
      // starting at addr into a set of stmread_word calls.  For now, the
      // range must be aligned on a sizeof(T) boundary, and T must be 1, 4,
      // or 8 bytes.
      TM_INLINE
      static T read(T* addr, TxThread* thread)
      {
          InvalidTypeAsSecondTemplateParameter itastp;
          T invalid = (T)itastp;
          return invalid;
      }

      // same as read, but for writes
      TM_INLINE
      static void write(T* addr, T val, TxThread* thread)
      {
          InvalidTypeAsSecondTemplateParameter itaftp;
          T invalid = (T)itaftp;
      }
  };

#if defined(STM_BITS_32)
  /*** standard dispatch for 4-byte types, since 8 bytes is the word size */
  template <typename T>
  struct DISPATCH<T, 4>
  {
      TM_INLINE
      static T read(T* addr, TxThread* thread)
      {
          return (T)(uintptr_t)thread->tmread(thread, (void**)addr
                                              STM_MASK(~0x0));
      }

      TM_INLINE
      static void write(T* addr, T val, TxThread* thread)
      {
          thread->tmwrite(thread, (void**)addr, (void*)(uintptr_t)val
                          STM_MASK(~0x0));
      }
  };

  /*** specialization for float */
  template <>
  struct DISPATCH<float, 4>
  {
      TM_INLINE
      static float read(float* addr, TxThread* thread)
      {
          union { float f;  void* v;  } v;
          v.v = thread->tmread(thread, (void**)addr STM_MASK(~0x0));
          return v.f;
      }

      TM_INLINE
      static void write(float* addr, float val, TxThread* thread)
      {
          union { float f;  void* v;  } v;
          v.f = val;
          thread->tmwrite(thread, (void**)addr, v.v STM_MASK(~0x0));
      }
  };

  /*** specialization for const float */
  template <>
  struct DISPATCH<const float, 4>
  {
      TM_INLINE
      static float read(const float* addr, TxThread* thread)
      {
          union { float f;  void* v;  } v;
          v.v = thread->tmread(thread, (void**)addr STM_MASK(~0x0));
          return v.f;
      }

      TM_INLINE
      static void write(const float*, float, TxThread*)
      {
          UNRECOVERABLE("You should not be writing a const float!");
      }
  };

  /*** specialization for 8-byte types... need to do two reads/writes */
  template <typename T>
  struct DISPATCH<T, 8>
  {
      TM_INLINE
      static T read(T* addr, TxThread* thread)
      {
          // get second word's address
          void** addr2 = (void**)((long)addr + 4);
          union {
              long long l;
              struct { void* v1; void* v2; } v;
          } v;
          // read the two words
          v.v.v1 = thread->tmread(thread, (void**)addr STM_MASK(~0x0));
          v.v.v2 = thread->tmread(thread, addr2 STM_MASK(~0x0));
          return (T)v.l;
      }

      TM_INLINE
      static void write(T* addr, T val, TxThread* thread)
      {
          // compute the two addresses
          void** addr1 = (void**)addr;
          void** addr2 = (void**)((long)addr + 4);
          // turn the value into two words
          union {
              T t;
              struct { void* v1; void* v2; } v;
          } v;
          v.t = val;
          // write the two words
          thread->tmwrite(thread, addr1, v.v.v1 STM_MASK(~0x0));
          thread->tmwrite(thread, addr2, v.v.v2 STM_MASK(~0x0));
      }
  };

  /**
   * specialization for doubles... just like other 8-byte, but with an extra
   * cast
   */
  template <>
  struct DISPATCH<double, 8>
  {
      TM_INLINE
      static double read(double* addr, TxThread* thread)
      {
          // get second word's address
          void** addr2 = (void**)((long)addr + 4);
          union {
              double t;
              struct { void* v1; void* v2; } v;
          } v;
          // read the two words
          v.v.v1 = thread->tmread(thread, (void**)addr STM_MASK(~0x0));
          v.v.v2 = thread->tmread(thread, addr2 STM_MASK(~0x0));
          return v.t;
      }

      TM_INLINE
      static void write(double* addr, double val, TxThread* thread)
      {
          // compute the two addresses
          void** addr1 = (void**) addr;
          void** addr2 = (void**) ((long)addr + 4);
          // turn the value into two words
          union {
              double t;
              struct { void* v1; void* v2; } v;
          } v;
          v.t = val;
          // write the two words
          thread->tmwrite(thread, addr1, v.v.v1 STM_MASK(~0x0));
          thread->tmwrite(thread, addr2, v.v.v2 STM_MASK(~0x0));
      }
  };

  /*** specialization for const double */
  template <>
  struct DISPATCH<const double, 8>
  {
      TM_INLINE
      static double read(const double* addr, TxThread* thread)
      {
          // get the second word's address
          void** addr2 = (void**)((long)addr + 4);
          union {
              double t;
              struct { void* v1; void* v2; } v;
          } v;
          // read the two words
          v.v.v1 = thread->tmread(thread, (void**)addr STM_MASK(~0x0));
          v.v.v2 = thread->tmread(thread, addr2 STM_MASK(~0x0));
          return v.t;
      }

      TM_INLINE
      static void write(const double*, double, TxThread*)
      {
          UNRECOVERABLE("You should not be writing a const double!");
      }
  };

  /**
   *  specialization for 1-byte types... must operate on the enclosing word.
   *
   *  NB: this can lead to granularity bugs if a byte is accessed
   *      nontransactionally while an adjacent byte is accessed
   *      transactionally.
   */
  template <typename T>
  struct DISPATCH<T, 1>
  {
      TM_INLINE
      static T read(T* addr, TxThread* thread)
      {
          // we must read the word (as a void*) that contains the byte at
          // address addr, then treat that as an array of T's from which we
          // pull out a specific element (based on masking the last two
          // bits)
          union { char v[4]; void* v2; } v;
          void** a = (void**)(((long)addr) & ~3);
          long offset = ((long)addr) & 3;
          v.v2 = thread->tmread(thread, a STM_MASK(0xFF << (8 * offset)));
          return (T)v.v[offset];
      }

      TM_INLINE
      static void write(T* addr, T val, TxThread* thread)
      {
          // to protect granularity, we need to read the whole word and
          // then write a byte of it
          union { T v[4]; void* v2; } v;
          void** a = (void**)(((long)addr) & ~3);
          long offset = ((long)addr) & 3;
          // read the enclosing word
          v.v2 = thread->tmread(thread, a STM_MASK(0xFF << (8 * offset)));
          v.v[offset] = val;
          thread->tmwrite(thread, a, v.v2 STM_MASK(0xFF << (8 * offset)));
      }
  };

#elif defined(STM_BITS_64)
  /*** standard dispatch for 8-byte types, since 8 bytes is the word size */
  template <typename T>
  struct DISPATCH<T, 8>
  {
      TM_INLINE
      static T read(T* addr, TxThread* thread)
      {
          return (T)(uintptr_t)thread->tmread(thread, (void**)addr
                                              STM_MASK(~0x0));
      }

      TM_INLINE
      static void write(T* addr, T val, TxThread* thread)
      {
          thread->tmwrite(thread, (void**)addr, (void*)(uintptr_t)val
                          STM_MASK(~0x0));
      }
  };

  /*** specialization for double */
  template <>
  struct DISPATCH<double, 8>
  {
      TM_INLINE
      static double read(double* addr, TxThread* thread)
      {
          union { double d;  void*  v; } v;
          v.v = thread->tmread(thread, (void**)addr STM_MASK(~0x0));
          return v.d;
      }

      TM_INLINE
      static void write(double* addr, double val, TxThread* thread)
      {
          union { double d;  void*  v; } v;
          v.d = val;
          thread->tmwrite(thread, (void**)addr, v.v STM_MASK(~0x0));
      }
  };

  /*** specialization for const double */
  template <>
  struct DISPATCH<const double, 8>
  {
      TM_INLINE
      static double read(const double* addr, TxThread* thread)
      {
          union { double d;  void*  v; } v;
          v.v = thread->tmread(thread, (void**)addr STM_MASK(~0x0));
          return v.d;
      }

      TM_INLINE
      static void write(const double*, double, TxThread*)
      {
          UNRECOVERABLE("You should not be writing a const double!");
      }
  };

  /*** specialization for long-double */
  template <>
  struct DISPATCH<long double, 16>
  {
      TM_INLINE
      static double read(double* addr, TxThread* thread)
      {
          // get second word's address
          void** addr2 = (void**)((long)addr + 4);
          union {
              double t;
              struct { void* v1; void* v2; } v;
          } v;
          // read the two words
          v.v.v1 = thread->tmread(thread, (void**)addr STM_MASK(~0x0));
          v.v.v2 = thread->tmread(thread, addr2 STM_MASK(~0x0));
          return v.t;
      }

      TM_INLINE
      static void write(double* addr, double val, TxThread* thread)
      {
          // compute the two addresses
          void** addr1 = (void**) addr;
          void** addr2 = (void**) ((long)addr + 4);
          // turn the value into two words
          union {
              double t;
              struct { void* v1; void* v2; } v;
          } v;
          v.t = val;
          // write the two words
          thread->tmwrite(thread, addr1, v.v.v1 STM_MASK(~0x0));
          thread->tmwrite(thread, addr2, v.v.v2 STM_MASK(~0x0));
      }
  };

  /**
   *  Since 4-byte types are sub-word, and since we do everything at the
   *  granularity of words, we need to do some careful work to make a 4-byte
   *  read/write work correctly.
   *
   *  NB: We're going to assume that 4-byte reads are always aligned
   *
   *  NB: This can lead to granularity bugs if a 4-byte value is accessed
   *      transactionally while a neighboring 4-byte value is accessed
   *      nontransactionally
   */

  /*** specialization for generic 4-byte types */
  template <typename T>
  struct DISPATCH<T, 4>
  {
      TM_INLINE
      static T read(T* addr, TxThread* thread)
      {
          // we must read the word (as a void*) that contains the 4byte at
          // address addr, then treat that as an array of T's from which we
          // pull out a specific element (based on the 3-lsb)
          union { int v[2]; void* v2; } v;
          void** a = (void**)(((intptr_t)addr) & ~7ul);
          long offset = (((intptr_t)addr)>>2)&1;
          v.v2 = thread->tmread(thread, a
                                STM_MASK(0xffffffff << (32 * offset)));
          return (T)v.v[offset];
      }

      TM_INLINE
      static void write(T* addr, T val, TxThread* thread)
      {
          // to protect granularity, we need to read the whole word and
          // then write a byte of it
          union { T v[2]; void* v2; } v;
          void** a = (void**)(((intptr_t)addr) & ~7ul);
          int offset = (((intptr_t)addr)>>2) & 1;
          // read the enclosing word
          v.v2 = thread->tmread(thread, a
                                STM_MASK(0xffffffff << (32 * offset)));
          v.v[offset] = val;
          thread->tmwrite(thread, a, v.v2
                          STM_MASK(0xffffffff << (32 * offset)));
      }
  };

  /*** specialization for floats */
  template <>
  struct DISPATCH<float, 4>
  {
      TM_INLINE
      static float read(float* addr, TxThread* thread)
      {
          // read the word as a void*, pull out the right portion of it
          union { float v[2]; void* v2; } v;
          void** a = (void**)(((intptr_t)addr)&~7ul);
          long offset = (((intptr_t)addr)>>2)&1;
          v.v2 = thread->tmread(thread, a
                                STM_MASK(0xffffffff << (32 * offset)));
          return v.v[offset];
      }

      TM_INLINE
      static void write(float* addr, float val, TxThread* thread)
      {
          // read the whole word, then write a word of it
          union { float v[2]; void* v2; } v;
          void**a = (void**)(((intptr_t)addr) & ~7ul);
          int offset = (((intptr_t)addr)>>2) & 1;
          // read enclosing word
          v.v2 = thread->tmread(thread, a
                                STM_MASK(0xffffffff << (32 * offset)));
          v.v[offset] = val;
          thread->tmwrite(thread, a, v.v2
                          STM_MASK(0xffffffff << (32 * offset)));
      }
  };

  /*** specialization for const float */
  template <>
  struct DISPATCH<const float, 4>
  {
      TM_INLINE
      static float read(const float* addr, TxThread* thread)
      {
          // read the word as a void*, pull out the right portion of it
          union { float v[2]; void* v2; } v;
          void** a = (void**)(((intptr_t)addr)&~7ul);
          long offset = (((intptr_t)addr)>>2)&1;
          v.v2 = thread->tmread(thread, a
                                STM_MASK(0xffffffff << (32 * offset)));
          return v.v[offset];
      }

      TM_INLINE
      static void write(const float*, float, TxThread*)
      {
          UNRECOVERABLE("You should not be writing a const float!");
      }
  };

  template <typename T>
  struct DISPATCH<T, 1>
  {
      TM_INLINE
      static T read(T* addr, TxThread* thread)
      {
          // we must read the word (as a void*) that contains the byte at
          // address addr, then treat that as an array of T's from which we
          // pull out a specific element (based on masking the last three
          // bits)
          union { char v[8]; void* v2; } v;
          void** a = (void**)(((long)addr) & ~7);
          long offset = ((long)addr) & 7;
          v.v2 = thread->tmread(thread, a
                                STM_MASK(0xffffffff << (8 * offset)));
          return (T)v.v[offset];
      }

      TM_INLINE
      static void write(T* addr, T val, TxThread* thread)
      {
          // to protect granularity, we need to read the whole word and
          // then write a byte of it
          union { T v[8]; void* v2; } v;
          void** a = (void**)(((long)addr) & ~7);
          long offset = ((long)addr) & 7;
          // read the enclosing word
          v.v2 = thread->tmread(thread, a
                                STM_MASK(0xffffffff << (8 * offset)));
          v.v[offset] = val;
          thread->tmwrite(thread, a, v.v2
                          STM_MASK(0xffffffff << (8 * offset)));
      }
  };

#else
#error Cannot figure out the right dispatch mechanism
#endif

} // namespace stm

#endif // API_LIBRARY_INST_HPP__
