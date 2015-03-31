/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  barrier.hpp
 *
 *  Simple barrier.  Currently based on pthread locks.
 *  Note that pthread barriers are an optional part of the standard, and
 *  are not supported on some platforms (including my mac).
 *  If verbose, prints time when barrier is achieved.
 */

#ifndef BARRIER_HPP__
#define BARRIER_HPP__

#include <pthread.h>    // for pthread_cond_t and pthread_mutex_t
#include <string>
    using std::string;

#include "macros.hpp"

class barrier {
    int participants;
    int parity;         // for sense reversal
    pthread_mutex_t mutex;
    int count[2];
    pthread_cond_t sem[2];
public:
    void wait(string s) {
        VERIFY(pthread_mutex_lock(&mutex));
        int my_parity = parity;
        ++count[my_parity];
        if (count[my_parity] == participants) {
            // I was the last to arrive
            count[my_parity] = 0;
            parity = 1 - my_parity;

            if (verbose && s.size() != 0) {
                unsigned long long now = getElapsedTime();
                {
                    with_lock cs(io_lock);
                    cout << "time: " << (now - start_time)/1e9 << " "
                                     << (now - last_time)/1e9
                                     << " (" << s << ")\n";
                }
                last_time = now;
            }
            VERIFY(pthread_cond_broadcast(&sem[my_parity]));
        } else {
            while (!count[my_parity] == 0) {
                VERIFY(pthread_cond_wait(&sem[my_parity], &mutex));
            }
        }
        VERIFY(pthread_mutex_unlock(&mutex));
    }
    barrier(int n) : participants(n) {
        count[0] = count[1] = 0;
        parity = 0;
        VERIFY(pthread_mutex_init(&mutex, 0));
        VERIFY(pthread_cond_init(&sem[0], 0));
        VERIFY(pthread_cond_init(&sem[1], 0));
    }
    ~barrier() {
        VERIFY(pthread_mutex_destroy(&mutex));
        VERIFY(pthread_cond_destroy(&sem[0]));
        VERIFY(pthread_cond_destroy(&sem[1]));
    }
};

#endif // BARRIER_HPP__
