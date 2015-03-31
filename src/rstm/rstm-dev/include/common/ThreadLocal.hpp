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
 *  Since Apple doesn't support __thread in its toolchain, we need a clean
 *  interface that lets us use either __thread or pthread_getspecific.  This
 *  file hides all interaction with thread-local storage behind a simple
 *  macro, so that the complexities of non-__thread are hidden from the
 *  programmer
 *
 *  Note, too, that we allow a non-Apple user to configure the library in
 *  order to explicitly use pthread_getspecific.
 */

// NB: This file could use significant hardening, using template
//     metaprogramming to support all the necessary types, i.e., arrays,
//     unions, etc.

#ifndef STM_COMMON_THREAD_LOCAL_H
#define STM_COMMON_THREAD_LOCAL_H

#include <stm/config.h>

/**
 *  We define the following interface for interacting with thread local data.
 *
 *    THREAD_LOCAL_DECL_TYPE(X)
 *
 *  The macro will expand in a platform specific manner into the correct
 *  thread local type for X.
 *
 *  Examples:
 *
 *              static THREAD_LOCAL_DECL_TYPE(unsigned) a;
 *    Linux:    static __thread unsigned a;
 *    Windows:  static __declspec(thread) unsigned a;
 *    pthreads: static ThreadLocal<unsigned, sizeof(unsigned)> a;
 *
 *              extern THREAD_LOCAL_DECL_TYPE(Foo*) foo;
 *    Linux:    extern __thread Foo* foo;
 *    Windows:  extern __declspec(thread) Foo* foo;
 *    pthreads: extern ThreadLocal<Foo*, sizeof(Foo*)> a;
 */
#if defined(STM_TLS_PTHREAD)
# define THREAD_LOCAL_DECL_TYPE(X) stm::tls::ThreadLocal<X, sizeof(X)>
#elif defined(STM_OS_WINDOWS)
# define THREAD_LOCAL_DECL_TYPE(X) __declspec(thread) X
#elif defined(STM_CC_GCC) || defined(STM_CC_SUN)
# define THREAD_LOCAL_DECL_TYPE(X) __thread X
#else
# warning "No thread local implementation defined."
#endif

/**
 *  In the above macro definitions, only STM_TLS_PTHREAD needs more work.
 *  The remainder of this file implements the ThreadLocal<> templates that
 *  make pthread_getspecific and pthread_setspecific look like __thread to
 *  client code
 */
#if defined(STM_TLS_PTHREAD)

#include <pthread.h>
#include <cstdlib>

namespace stm
{
  namespace tls
  {
    /**
     *  The basic thread local wrapper. The pthread interface stores the
     *  value as a void*, and this class manages that void* along with the
     *  pthread key.
    */
    class PThreadLocalImplementation
    {
      protected:
        PThreadLocalImplementation(void* const v)
        {
            pthread_key_create(&key, NULL);
            pthread_setspecific(key, v);
        }

        virtual ~PThreadLocalImplementation() { pthread_key_delete(key); }
        void* getValue() const { return pthread_getspecific(key); }
        void setValue(void* const v) { pthread_setspecific(key, v); }

      private:
        pthread_key_t key;

      private:
        // Do not implement
        PThreadLocalImplementation();
        PThreadLocalImplementation(const PThreadLocalImplementation&);

        PThreadLocalImplementation&
        operator=(const PThreadLocalImplementation&);
    };

    /**
     *  Templates allow us to mimic an __thread interface to PThread data. We
     *  have two basic categories of data.
     *
     *   1) Value data.
     *   2) Pointer data.
     *
     *  Value data is builtin types and user defined structs that are
     *  compatible with direct __thread allocation. We can split this type of
     *  data into two cases.
     *
     *   1) Data that can fit in the size of a void*.
     *   2) Data that is too large.
     *
     *  This distinction is important when we consider levels of
     *  indirection. The pthread interface gives us access to void* sized
     *  slots of data. If we can fit what we need there, then we have just
     *  the one level of indirection to access it. If we can't, then we need
     *  to allocate space elsewhere for it, and store a pointer to that space
     *  in the slot.
     *
     *  Pointer data is easy to manage, since the client expects the location
     *  to look like a pointer, and pthreads is giving us a pointer. The
     *  client is going to have to manage the memory if it's dynamically
     *  allocated, so we can just return it as needed.
     *
     *  The main problem to this interface is that each interaction requires
     *  a pthread library call. If the client knew there was a pthreads
     *  interface (or just an interface more expensive than __thread)
     *  underneath then it could optimize for that situation.
     */

    /**
     *  The ThreadLocal template for objects that are larger than the size
     *  allotted by a pthread_getspecific. It uses either malloc and free or
     *  a trivial new constructor to allocate and deallocate space for the
     *  data, and memcpy to write to the data as needed. It owns the
     *  allocated space, which is fine because the client is thinking of this
     *  as automatically managed anyway.
     *
     *  Currently, we trigger an error if the type has a constructor, but
     *  doesn't have a default constructor. This is the same approach that
     *  C++ takes with arrays of user-defined classes.
     */
    template <typename T, unsigned S>
    class ThreadLocal : public PThreadLocalImplementation
    {
      public:
        ThreadLocal() : PThreadLocalImplementation(new T()) { }

        ThreadLocal(T t) : PThreadLocalImplementation(new T())
        {
            __builtin_memcpy(getValue(), &t, S);
        }

        virtual ~ThreadLocal() { delete static_cast<T*>(getValue()); }

        T* operator&() const { return static_cast<T*>(getValue()); }

      private:
        // Do not implement these. We assume that anyone trying to copy the
        // ThreadLocal object probably wants to copy the underlying object
        // instead.
        ThreadLocal(const ThreadLocal<T, S>&);
        ThreadLocal<T, S>& operator=(const ThreadLocal<T, S>&);
    };

    /**
     *  The ThreadLocal template for objects that are the size of a void*,
     *  but not a pointer. This differs from the basic template in that we
     *  don't need to allocate any extra space for the stored item.
     */
    template <typename T>
    class ThreadLocal<T, sizeof(void*)> : public PThreadLocalImplementation
    {
      public:
        ThreadLocal() : PThreadLocalImplementation(NULL) { }

        ThreadLocal(T t) : PThreadLocalImplementation(NULL)
        {
            __builtin_memcpy(getValue(), &t, sizeof(T));
        }

        T* operator&() const { return static_cast<T*>(getValue()); }

      private:
        // Do not implement these. We assume that anyone trying to copy the
        // ThreadLocal object probably wants to copy the underlying object
        // instead.
        ThreadLocal(const ThreadLocal<T, sizeof(void*)>&);

        ThreadLocal<T,sizeof(void*)>&
        operator=(const ThreadLocal<T,sizeof(void*)>&);
    };

    /**
     *  We use partial template specialization to implement a thread local
     *  type just for pointers. This extends the interface to allow
     *  interaction with the stored variable in "smart pointer" fashion.
     *
     *  This differs from the basic thread local implementation in that we
     *  don't provide an address-of operator, in the expectation that no one
     *  is going to want it, but we do provide an implicit cast to the
     *  underlying pointer type that returns the pointer value stored at the
     *  key.
     *
     *  This allows clients to pass and return the value as expected. A
     *  normal smart pointer would be hesitant to do this because of
     *  ownership issues, but this class is really just trying to emulate
     *  __thread. The ThreadLocal does *not* take ownership of managing the
     *  underlying pointer.
     */
    template <typename T>
    class ThreadLocal<T*, sizeof(void*)> : public PThreadLocalImplementation
    {
      public:
        ThreadLocal(T* t = NULL) : PThreadLocalImplementation(t) { }

        virtual ~ThreadLocal() { }

        // The smart pointer interface to the variable.
        const T& operator*() const { return *static_cast<T*>(getValue()); }
        const T* operator->() const { return static_cast<T*>(getValue()); }
        T& operator*() { return *static_cast<T*>(getValue()); }
        T* operator->() { return static_cast<T*>(getValue()); }
        operator T*() { return static_cast<T*>(getValue()); }

        // allow assignments
        ThreadLocal<T*, sizeof(void*)>& operator=(T* rhs) {
            setValue(rhs);
            return *this;
        }

        bool operator==(T* rhs) { return (getValue() == rhs); }

      private:
        // Restrict access to potentially dangerous things. Start by
        // preventing the thread local to be copied around (presumably people
        // trying to copy a ThreadLocal /actually/ want to copy the
        // underlying object).
        ThreadLocal(const ThreadLocal<T*, sizeof(T*)>&);

        ThreadLocal<T*, sizeof(T*)>&
        operator=(const ThreadLocal<T*, sizeof(T*)>&);
    };
  } // namespace stm::tls
} // namespace stm
#endif

#endif // STM_COMMON_THREAD_LOCAL_H
