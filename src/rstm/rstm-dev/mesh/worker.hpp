/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  worker.hpp
 *
 *  Main file for the parallel solver.
 */

#ifndef WORKER_HPP__
#define WORKER_HPP__

#include "common.hpp"
#include "point.hpp"
#include "edge.hpp"
#include "queues.hpp"
#include "my_thread.hpp"
#include "barrier.hpp"

// Everything regions (vertical stripes) need to know about each other.
//
struct region_info {
    point** points;
    int *counts;
    int npts;
    point* leftmost;
    point* rightmost;
    simple_queue<edge*>* tentative_edges;

    region_info(const int tid) {
        tentative_edges = new concurrent_queue<edge*>(tid);
        points = new point*[num_workers];
        counts = new int[num_workers];
        npts = 0;
        leftmost = rightmost = 0;
    }
    ~region_info() {
        delete[] points;
        delete[] counts;
    }
};

class worker : public runnable {
    int col;
    region_info **regions;
    barrier *bar;
public:
    virtual void operator()();
    worker(int c, region_info **r, barrier *b)
        : col(c), regions(r), bar(b) { }
};

#endif // WORKER_HPP__
