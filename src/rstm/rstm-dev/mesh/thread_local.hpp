/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef THREADLOCAL_HPP__
#define THREADLOCAL_HPP__

/**
 *  To use a ThreadLocalPointer, declare one in a header like this:
 *    extern ThreadLocalPointer<foo> my_foo;
 *
 *  Then you must back the pointer in a cpp file like this:
 *    ThreadLocalPointer<foo> currentHeap;
 *    #if defined(LOCAL_POINTER_ANNOTATION)
 *    template <> LOCAL_POINTER_ANNOTATION foo*
 *    ThreadLocalPointer<foo>::thr_local_key = NULL;
 *    #endif
 */

/**
 *  This #if block serves three roles:
 *    - It includes files as necessary for different pthread implementations
 *    - It hides differences between VisualC++ and GCC syntax
 *    - It makes sure that a valid thread-local option is specified so that
 *      subsequent #ifs don't have to do error checking
 */

#if defined(__GNUC__)
#define LOCAL_POINTER_ANNOTATION      __thread
#elif defined(_MSC_VER)
#include <windows.h>
#define LOCAL_POINTER_ANNOTATION      __declspec(thread)
#else
#include <pthread.h>
#endif

  /**
   *  Hide the fact that some of our platforms use pthread_getspecific while
   *  others use os-specific thread local storage mechanisms.  In all cases, if
   *  you instantiate one of these with a T, then you'll get a thread-local
   *  pointer to a T that is easy to access and platform independent
   */
  template<class T>
  class ThreadLocalPointer
  {
      /**
       * either declare a key for interfacing to PTHREAD thread local
       * storage, or else declare a static thread-local var of type T*
       */
#if defined(TLS_PTHREAD)
      pthread_key_t thr_local_key;
#else
      static LOCAL_POINTER_ANNOTATION T* thr_local_key;
#endif

    public:
      /**
       *  Perform global initialization of the thread key, if applicable.  Note
       *  that we have a default value of NULL.  You can't store a NULL value
       *  in this thread-local pointer if you want to do subsequent tests to
       *  ensure that the variable is initialized for your thread.
       */
      ThreadLocalPointer()
      {
#if defined(TLS_PTHREAD)
          pthread_key_create(&thr_local_key, NULL);
#else
          thr_local_key = NULL;
#endif
      }

      /*** Free the thread local key or set the pointer to NULL */
      ~ThreadLocalPointer()
      {
#if defined(TLS_PTHREAD)
          pthread_key_delete(thr_local_key);
#else
          thr_local_key = NULL;
#endif
      }

      /*** Get the pointer stored for this thread */
      T* get() const
      {
#if defined(TLS_PTHREAD)
          return static_cast<T* const>(pthread_getspecific(thr_local_key));
#else
          return thr_local_key;
#endif
      }

      /***  Set this thread's pointer */
      void set(T* val)
      {
#if defined(TLS_PTHREAD)
          pthread_setspecific(thr_local_key, (void*)val);
#else
          thr_local_key = val;
#endif
      }

      /*** operators for dereferencing */
      T* operator->() const { return get(); }
      T& operator*() const { return *get(); }
  };

#endif // THREADLOCAL_HPP__
