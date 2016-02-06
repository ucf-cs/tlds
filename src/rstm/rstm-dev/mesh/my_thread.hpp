/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  my_thread.hpp     (the name "thread.h" is taken)
 *
 *  Simple wrapper for pthreads.
 *  Currently uses default attributes, except for PTHREAD_SCOPE_SYSTEM,
 *  which allows threads to be scheduled on multiple processors.
 */

#ifndef MY_THREAD_HPP__
#define MY_THREAD_HPP__

#include <pthread.h>
#include <iostream>
    using std::cout;
#include <sstream>
    using std::stringstream;
#include <cassert>

#include "common.hpp"
#include "macros.hpp"
#include "lock.hpp"

/* abstract */
class runnable {
public:
    virtual void operator()() = 0;
    virtual ~runnable() { }
};

extern void *call_runnable(void *f);

class thread {
    pthread_t my_pthread;
    runnable* my_runnable;
    friend void *call_runnable(void *f);
    int transaction_count;
public:
    stringstream vout;
        // Stream to which to send verbose output when stuck inside a txn.
        // Not synchronized; should be used ONLY by the owner thread.

    void enter_transaction() {
        transaction_count++;
    }
    void leave_transaction() {
        transaction_count--;
    }
    bool in_transaction() {
        return transaction_count > 0;
    }

    // start immediately upon creation:
    thread(runnable* f) {
        my_runnable = f;
        transaction_count = 0;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
        VERIFY(pthread_create(&my_pthread, &attr, call_runnable, (void*) this));
    }
    thread() {
        // for main program
        my_runnable = 0;
        transaction_count = 0;
        my_pthread = pthread_self();
        THREAD_SHUTDOWN();
    }
    TRANSACTION_PURE void erase_buffered_output() {
        (void) vout.str(std::string());
        vout.clear();
    }
    void dump_buffered_output() {
        with_lock cs(io_lock);
        cout << vout.str();
    }

    // join by deleting:
    ~thread() {
        assert(my_runnable);        // not the main thread
        VERIFY(pthread_join(my_pthread, 0));
    }
};

#endif // MY_THREAD_HPP__
