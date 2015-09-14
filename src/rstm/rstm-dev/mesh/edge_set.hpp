/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  edge_set.hpp
 *
 *  Concurrent set of edges.
 *  Good memory locality if your work is confined to a worker stripe.
 *
 *  Thread-safe insert, erase, and contains methods.
 *  Insert and erase log to cout if output_incremental.
 *
 *  Non-thread-safe print_all method.
 */

#ifndef EDGE_SET_HPP__
#define EDGE_SET_HPP__

#include "tm_hash_set.hpp"
    // I'd use tr1::unordered_set<edge*>, but icc isn't willing to accept
    // its insert and erase routines as [[transaction_safe]].

#include "edge.hpp"

class edge_set {
    typedef tm_hash_set<edge*> segment_t;
    segment_t** segments;
public:
    TRANSACTION_SAFE void insert(edge* e);
    TRANSACTION_SAFE void erase(edge* e);
    bool contains(edge* e) const;

    // for final output, if desired (unsynchronized)
    void print_all() const;

    edge_set();
        // constructor creates segments but does not initialize them
    void help_initialize(int col);
        // initialize segments in parallel with other threads

    ~edge_set();
};

extern edge_set *edges;

#endif // EDGE_SET_HPP__
