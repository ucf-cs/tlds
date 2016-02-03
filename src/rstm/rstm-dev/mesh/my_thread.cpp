/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  my_thread.cpp
 *
 *  Simple wrapper for pthreads.
 *  Currently uses default attributes, except for PTHREAD_SCOPE_SYSTEM,
 *  which allows threads to be scheduled on multiple processors.
 */

#include "my_thread.hpp"

void *call_runnable(void *s) {
    THREAD_INIT();
    thread* self = static_cast<thread*>(s);
    currentThread = self;
    (*(self->my_runnable))();
    return 0;
}
