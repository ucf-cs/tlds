/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  edge_set.cpp
 *
 *  Concurrent set of edges.
 *  Good memory locality if your work is confined to a worker stripe.
 *  Has 2*numWorkers hash_sets inside.
 *
 *  Thread-safe insert, erase, and contains methods.
 *  All are currently implemented using pthread_mutex locks.
 *  Insert and erase log to cout (or currentThread->vout) if output_incremental.
 *
 *  Non-thread-safe print_all methods.
 */

#include <iostream>
    using std::cout;

#include "common.hpp"
#include "point.hpp"
#include "edge_set.hpp"

static TRANSACTION_SAFE int segment(edge* e) {
    point* p0 = e->points[0];
    point* p1 = e->points[1];

    int a = bucket(p0);
    int b = bucket(p1);
    return a < b ? a : b;
}

void edge_set::insert(edge* e) {
    int s = segment(e);
    segments[s]->insert(e);
    if (output_incremental) {
        e->print("+ ");
    }
}

void edge_set::erase(edge* e) {
    int s = segment(e);
    segments[s]->remove(e);
    if (output_incremental) {
        e->print("- ");
    }
}

bool edge_set::contains(edge* e) const {
    int s = segment(e);
    return segments[s]->lookup(e);
}

static void print_edge(edge* e) { e->print("+ "); }

void edge_set::print_all() const {
    for (int i = 0; i < num_workers*2; i++) {
        segments[i]->apply_to_all(print_edge);
    }
}

edge_set::edge_set() {
    segments = new segment_t*[num_workers*2];
        // no initialization performed for pointers (non-class type)
}

void edge_set::help_initialize(int col) {
    // There will be 3*(n-1)-h edges in the final graph,
    // where n is num_points and h is the number of points
    // on the convex hull.  Factor of 4 here gives us an
    // expected load factor of just under 3/4
    col <<= 1;
    segments[col] = new segment_t((4*num_points)/num_workers/2);
    segments[col+1] = new segment_t((4*num_points)/num_workers/2);
}

edge_set::~edge_set() {
    for (int i = 0; i < num_workers; i++) {
        delete segments[i];
    }
    delete[] segments;
}
