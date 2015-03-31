/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  lock.hpp
 *
 *  Currently uses pthread mutex, with PTHREAD_MUTEX_RECURSIVE for
 *  re-entrancy.
 */

#ifndef LOCK_HPP__
#define LOCK_HPP__

#include <pthread.h>    // for pthread_mutex_t

#include "macros.hpp"

class d_lock {
    pthread_mutex_t mutex;
public:
    void acquire() {
        VERIFY(pthread_mutex_lock(&mutex));
    }
    void release() {
        VERIFY(pthread_mutex_unlock(&mutex));
    }
    d_lock() {
        pthread_mutexattr_t attrs;
        VERIFY(pthread_mutexattr_init(&attrs));
        VERIFY(pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE));
        VERIFY(pthread_mutex_init(&mutex, &attrs));
    }
    ~d_lock() {
        VERIFY(pthread_mutex_destroy(&mutex));
    }
};

// Declaring one of these makes a scope a critical section.
//
class with_lock {
    d_lock *my_lock;
public:
    with_lock(d_lock &l) {
        my_lock = &l;
        my_lock->acquire();
    }
    ~with_lock() {
        my_lock->release();
    }
};

#endif // LOCK_HPP__
