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
 *  The icc stm compiler is based on ICC 11.0, which has fake support for
 *  __sync builtins. It knows what they are, and emits __sync_..._N symbols
 *  for them, but does not implement them, even when given a -march=i686 or
 *  greater.
 *
 *  http://origin-software.intel.com/en-us/forums/showthread.php?t=71183
 *
 *  The version 4 compiler that we used is still based on 11.0 (610), and has
 *  the same limitation. We provide our own support in this file.
 */

#ifndef STM_COMMON_PLATFORM_ICC_FIXES_HPP
#define STM_COMMON_PLATFORM_ICC_FIXES_HPP

// only include this file when building with ICC
#ifndef __ICC
#   error "icc-sync.hpp included incorrectly"
#endif

#include <cstddef>  // size_t
#include <stdint.h> // uintptr_t

/**
 *  We're going to do some basic metaprogramming to emulate the interface
 *  that we expect from the __sync builtins, i.e., if there is a correct
 *  __sync for the type, then we'll use it.
 */

/**
 *  Ultimately, what this file provides is a set of 5 macros that look, feel,
 *  and smell just like the __sync builtins.  Here are the macros, the
 *  implementations follow.
 */
#define __sync_synchronize()                            \
    asm volatile("mfence":::"memory")

#define __sync_bool_compare_and_swap(p, f, t)           \
    stm::iccsync::bool_compare_and_swap(p, f, t)

#define __sync_val_compare_and_swap(p, f, t)            \
    stm::iccsync::val_compare_and_swap(p, f, t)

#define __sync_lock_test_and_set(p, v)                  \
    stm::iccsync::lock_test_and_set(p, v)

#define __sync_fetch_and_add(p, a)                      \
    stm::iccsync::fetch_and_add(p, a)

namespace stm
{
  namespace iccsync
  {
    /**
     *  Our partial specialization helper is parameterized based on the type
     *  (T), the number of bytes in the type (N), and if this is a subword,
     *  word or double-word access (W). We assume that all addresses are
     *  aligned.
     *
     *    T is necessary so that the caller doesn't need to perform any casts.
     *
     *    N is necessary because our implementation depends on the number of
     *    bytes.
     *
     *    W allows us to deduce the platform without ifdefing anything.
     *
     *  NB: We've only implemented partial templates for what we actually
     *      use. Extending this is straightforward.
     */
    template <typename T,
              size_t W = sizeof(void*),
              size_t N = sizeof(T)>
    struct SYNC { };

    /**
     *  The byte implementations.
     */
    template <typename T, size_t W>
    struct SYNC<T, W, 1>
    {
        static T swap(volatile T* address, T value)
        {
            asm volatile("lock xchgb %[value], %[address]"
                         : [value]   "+q" (value),
                           [address] "+m" (*address)
                         :
                         : "memory");
            return value;
        }
    };

    /**
     *  The halfword sync implementations, independent of the platform
     *  bitwidths. We don't need any of these at the moment.
     */
    template <typename T, size_t W>
    struct SYNC<T, W, 2> { };

    /**
     * The word implementations, independent of the platform bitwidth.
     */
    template <typename T, size_t W>
    struct SYNC<T, W, 4>
    {
        static T swap(volatile T* address, T value)
        {
            asm volatile("lock xchgl %[value], %[address]"
                         : [value]   "+r" (value),
                           [address] "+m" (*address)
                         :
                         : "memory");
            return value;
        }

        // We can cas a word-sized value with a single x86 lock cmpxchgl
        static T cas(volatile T* addr, T from, T to)
        {
            asm volatile("lock cmpxchgl %[to], %[addr]"
                         :        "+a" (from),
                                  [addr] "+m" (*addr)
                         : [to]   "q"  (to)
                         : "cc", "memory");
            return from;
        }

        // We exploit the fact that xmpxchgl sets the Z flag
        static bool bcas(volatile T* addr, T from, T to)
        {
            bool result;
            asm volatile("lock cmpxchgl %[to], %[addr]\n\t"
                         "setz %[result]"
                         : [result] "=q" (result),
                           [addr]   "+m" (*addr),
                           "+a"(from)
                         : [to]     "q"  (to)
                         : "cc", "memory");
            return result;
        }
    };

    /**
     *  The doubleword implementations, for 4-byte platforms.
     */
    template <typename T>
    struct SYNC<T, 4, 8>
    {
        // implemented in terms of cas, because we don't have an 8 byte
        // xchg8b.
        static T swap(volatile T* address, T value)
        {
            // read memory, then update memory with value, making sure noone
            // wrote a new value---ABA is irrelevant. Can't use val cas
            // because I need to know if my memory write happened.
            T mem = *address;
            while (!bcas(address, mem, value))
                mem = *address;
            return mem;
        }

        // 64-bit CAS
        //
        // Our implementation depends on if we are compiling in a PIC or
        // non-PIC manner. PIC will not let us use %ebx in inline asm, so in
        // that case we need to save %ebx first, and then call the cmpxchg8b.
        //
        // * cmpxchg8b m64: Compare EDX:EAX with m64. If equal, set ZF and load
        //                  ECX:EBX into m64. Else, clear ZF and load m64 into
        //                  EDX:EAX.
        //
        // again, we exploit the Z-flag side effect here
        static bool bcas(volatile T* addr, T from, T to)
        {
            union {
                T from;
                uint32_t to[2];
            } cast = { to };

            bool result;
#if defined(__PIC__)
            asm volatile("xchgl %%ebx, %[to_low]\n\t"  // Save %ebx
                         "lock cmpxchg8b\t%[addr]\n\t" // Perform the exchange
                         "movl %[to_low], %%ebx\n\t"   // Restore %ebx
                         "setz %[result]"
                         : "+A" (from),
                           [result]  "=q" (result),
                           [addr]    "+m" (*addr)
                         : [to_high] "c"  (cast.to[1]),
                           [to_low]  "g"  (cast.to[0])
                         : "cc", "memory");
#else
            asm volatile("lock cmpxchg8b %[addr]\n\t"
                         "setz %[result]"
                         : "+A" (from),
                           [result] "=q" (result),
                           [addr]   "+m" (*addr)
                         : "c"  (cast.to[1]),
                           "b"  (cast.to[0])
                         : "cc", "memory");
#endif
            return result;
        }
    };

    /**
     *  The doubleword sync implementations, for 8-byte platforms.
     */
    template <typename T>
    struct SYNC<T, 8, 8>
    {
        static T swap(volatile T* address, T value)
        {
            asm volatile("lock xchgq %[value], %[address]"
                         : [value]   "+r" (value),
                           [address] "+m" (*address)
                         :
                         : "memory");
            return value;
        }

        // We can cas a word-sized value with a single x86 lock cmpxchgq
        static T cas(volatile T* addr, T from, T to)
        {
            asm volatile("lock cmpxchgq %[to], %[addr]"
                         : "+a" (from),
                           [addr] "+m" (*addr)
                         : [to]   "q"  (to)
                         : "cc", "memory");
            return from;
        }

        static bool bcas(volatile T* addr, T from, T to)
        {
            bool result;
            asm volatile("lock cmpxchgq %[to], %[addr]\n\t"
                         "setz %[result]"
                         : [result] "=q"  (result),
                           [addr]   "+m"  (*addr),
                                    "+a" (from)
                         : [to]     "q"   (to)
                         : "cc", "memory");
            return result;
        }
    };

    /**
     *  The quadword implementations, for 8-byte platforms. We don't ever
     *  need this at the moment.
     */
    template <typename T>
    struct SYNC<T, 8, 16> { };

    /**
     *  We're really liberal with the types of the parameters that we pass to
     *  the sync builtins. Apparently, gcc is fine with this, and "does the
     *  right thing" for them.  Our SYNC interface, on the other hand, is
     *  really picky about type matching. This CAS_ADAPTER does the type
     *  matching that we need for our stm usage.
     */

    /**
     *  This matches whenever T and S are different, and the value we're
     *  casting in may be smaller than the value we're casting to. This works
     *  because we've specialized for T == S, and for S > T.
     */
    template <typename T, typename S, size_t D = sizeof(T) / sizeof(S)>
    struct CAS_ADAPTER
    {
        static T cas(volatile T* address, S from, S to)
        {
            return SYNC<T>::cas(address, static_cast<T>(from),
                                static_cast<T>(to));
        }

        static bool bcas(volatile T* address, S from, S to)
        {
            return SYNC<T>::bcas(address, static_cast<T>(from),
                                 static_cast<T>(to));
        }
    };

    /*** well-typed compare-and-swap */
    template <typename T>
    struct CAS_ADAPTER<T, T, 1>
    {
        static T cas(volatile T* address, T from, T to)
        {
            return SYNC<T>::cas(address, from, to);
        }

        static bool bcas(volatile T* address, T from, T to)
        {
            return SYNC<T>::bcas(address, from, to);
        }
    };

    /**
     *  This is when the value is larger than the type we are casting to
     *
     *  NB: We don't know how to do this right now. We'll get a compile time
     *  error if anyone instantiates it.
     */
    template <typename T, typename S>
    struct CAS_ADAPTER<T, S, 0> { };

    /**
     *  An adapter class that matches types for swapping. This matches only
     *  when S is smaller than T, because equal sized and larger have their
     *  own specializations.
     */
    template <typename T, typename S, size_t D = sizeof(T) / sizeof(S)>
    struct SWAP_ADAPTER
    {
        static T swap(volatile T* address, S value)
        {
            return SYNC<T>::swap(address, static_cast<T>(value));
        }
    };

    /*** the "well-typed" swap */
    template <typename T>
    struct SWAP_ADAPTER<T, T, 1>
    {
        static T swap(volatile T* address, T value)
        {
            return SYNC<T>::swap(address, value);
        }
    };

    /**
     *  swap when S is the same size as a T... we could probaly do this more
     *  intelligently than with a c-cast
     */
    template <typename T, typename S>
    struct SWAP_ADAPTER<T, S, 1>
    {
        static T swap(volatile T* address, S value)
        {
            return SYNC<T>::swap(address, (T)value);
        }
    };

    /**
     *  swap when S is larger than a T, we have to intelligently cast.. just
     *  c-casting for now
     */
    template <typename T, typename S>
    struct SWAP_ADAPTER<T, S, 0>
    {
        static T swap(volatile T* address, S value)
        {
            return SYNC<T>::swap(address, (T)value);
        }
    };

    /**
     *  The primary interface to the lock_test_and_set primitive. Uses the
     *  parameter types to pick the correct swap adapter.
     */
    template <typename T, typename S>
    inline T lock_test_and_set(volatile T* address, S value)
    {
        return SWAP_ADAPTER<T, S>::swap(address, value);
    }

    /**
     *  The primary interface to the bool compare-and-swap routine. Uses the
     *  parameter types to pick the correct implementation.
     */
    template <typename T, typename S>
    inline bool bool_compare_and_swap(volatile T* address, S from, S to)
    {
        return CAS_ADAPTER<T, S>::bcas(address, from, to);
    }

    /**
     *  The primary interface to the val compare-and-swap routine. Uses the
     *  parameter types to pick the correct implementation.
     */
    template <typename T, typename S>
    inline T val_compare_and_swap(volatile T* address, S from, S to)
    {
        return CAS_ADAPTER<T, S>::cas(address, from, to);
    }

    /**
     *  We implement fetch_and_add implemented in terms of bcas. We actually
     *  don't have a problem with the type of the value parameter, as long as
     *  the T + S operator returns a T, which it almost always will.
     */
    template <typename T, typename S>
    inline T fetch_and_add(volatile T* address, S value)
    {
        T mem = *address;
        // NB: mem + value must be a T
        while (!SYNC<T>::bcas(address, mem, mem + value))
            mem = *address;
        return mem;
    }
  } // namespace stm::iccsync
} // namespace stm

#endif // STM_COMMON_PLATFORM_ICC_FIXES_HPP
