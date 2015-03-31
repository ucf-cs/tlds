/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  side.hpp
 *
 *  Data structure to represent one side of a stitching-up operation.
 *  Contains methods to move around the border of a convex hull.
 */

#ifndef SIDE_HPP__
#define SIDE_HPP__

#include "common.hpp"
#include "point.hpp"
#include "edge.hpp"

// Point p is assumed to lie on a convex hull.  Return the edge that
// lies dir of p on the hull.
//
TRANSACTION_SAFE edge* hull_edge(point* p, int dir);

struct pv_side {
    point* p;     // working point
    edge* a;      // where we came from
    edge* b;      // where we're going to
    point* ap;    // at far end of a
    point* bp;    // at far end of b
    int ai;       // index of p within a
    int bi;       // index of p within b
    int dir;      // which way are we moving?

    // No non-trivial constructor.

    // pt is a point on a convex hull.
    // Initialize such that a and b are the the edges with the
    // external angle, and b is d (ccw, cw) of a.
    //
    void initialize(point* pt, int d);

    // e is an edge between two regions in the process of being
    // stitched up.  pt is an endpoint of e.  After reinitialization,
    // a should be e, ai should be e->index_of(pt), and b should be d
    // of a at end ai.
    //
    void reinitialize(edge* e, point* pt, int d);

    // Nearby edges may have been updated.  Make sure a and
    // b still flank endpoints of mid in direction d.
    // This means that mid _becomes_ a.
    //
    void update(edge* mid, int d);

    // Move one edge along a convex hull (possibly with an external
    // edge attached at p), provided we stay in sight of point o.
    // Return edge we moved across, or null if unable to move.
    //
    edge* move(point* o);

    // We're stitching up a seam.  'This' captures one side of
    // current base edge (not explicitly needed).  Edge bottom is at
    // the "bottom" (initial end) of the seam; tells us if we cycle all
    // the way around.  Point o is at other end of base.  Find candidate
    // endpoint for new edge on this side, moving "up" the seam.
    // Break edges as necessary.
    //
    point* find_candidate(edge* bottom, point* o);

    // Move one edge along a convex hull (possibly with an external
    // edge attached at p), provided we stay in sight of point o
    // and don't get too close to the next seam.
    // Return edge we moved across, or null if we didn't move.
    //
    edge* conditional_move(point* o, int seam);
};

struct tx_side {
    point* p;     // working point
    edge* b;      // where we're going to
    point* bp;    // at far end of b
    int bi;       // index of p within b
    int dir;      // which way are we moving?

    // No non-trivial constructor.

    // e is an edge between two regions in the process of being
    // stitched up.  pt is an endpoint of e.  After initialization,
    // b should be d of e at the p end, bi should be the p end of b,
    // and bp should be at the other end of b.
    //
    TRANSACTION_SAFE void initialize(edge* e, point* pt, int d);

    // Move one edge along a convex hull (possibly with an external
    // edge attached at p), provided we stay in sight of point o.
    // Return edge we moved across, or null if unable to move.
    //
    TRANSACTION_SAFE edge* move(point* o);
};

#endif // SIDE_HPP__
