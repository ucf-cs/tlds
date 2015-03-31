/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  side.cpp
 *
 *  Data structure to represent one side of a stitching-up operation.
 *  Contains methods to move around the border of a convex hull.
 */

#include "side.hpp"

// Point p is assumed to lie on a convex hull.  Return the edge that
// lies dir of p on the hull.
//
edge* hull_edge(point* p, int dir)
{
    if (p->first_edge == 0) {
        return 0;    // NULL
    }
    else {
        edge* a = p->first_edge;
        int ai = a->index_of(p);
        point* ap = a->points[1-ai];
        if (a->neighbors[ai][dir] == a) {
            // only one incident edge at p
            return a;
        } else {
            // >= 2 incident edges at p;
            // need to find correct ones
            edge* b;
            while (true) {
                b = a->neighbors[ai][dir];
                int bi = b->index_of(p);
                point* bp = b->points[1-bi];
                if (extern_angle(ap, p, bp, dir))
                    return b;
                a = b;
                ai = bi;
                ap = bp;
            }
        }
    }
}

// pt is a point on a convex hull.
// Initialize such that a and b are the edges with the
// external angle, and b is dir (ccw, cw) of a.
//
void pv_side::initialize(point* pt, int d) {
    p = pt;
    dir = d;
    b = hull_edge(pt, d);
    if (b == 0) {
        a = 0;
        ap = 0;  bp = 0;
        ai = bi = 0;
    } else {
        bi = b->index_of(p);
        bp = b->points[1-bi];
        a = b->neighbors[bi][1-dir];
        ai = a->index_of(p);
        ap = a->points[1-bi];
    }
}

// e is an edge between two regions in the process of being
// stitched up.  pt is an endpoint of e.  After initialization,
// b should be d of e at the p end, bi should be the p end of b,
// and bp should be at the other end of b.
//
void tx_side::initialize(edge* e, point* pt, int d) {
    p = pt;
    dir = d;
    b = e->neighbors[e->index_of(p)][dir];
    bi = b->index_of(p);
    bp = b->points[1-bi];
}

// Nearby edges may have been updated.  Make sure a and
// b still flank endpoints of mid in direction dir.  This
// means that mid _becomes_ a.
//
void pv_side::update(edge* mid, int d) {
    dir = d;
    a = mid;
    ai = mid->index_of(p);
    b = mid->neighbors[ai][dir];
    bi = b->index_of(p);
    ap = mid->points[1-ai];
    bp = b->points[1-bi];
}

// e is an edge between two regions in the process of being
// stitched up.  pt is an endpoint of e.  After initialization,
// a should be e, ai should be e->index_of(pt), and b should be dir
// of a at end ai.
//
void pv_side::reinitialize(edge* e, point* pt, int d) {
    p = pt;
    update(e, d);
}

// Move one edge along a convex hull (possibly with an external
// edge attached at p), provided we stay in sight of point o.
// Return edge we moved across, or null if unable to move.
//
edge* pv_side::move(point* o) {
    if (b != 0 && bp != o && !extern_angle(bp, p, o, 1-dir)) {
        a = b;
        ai = 1-bi;
        ap = p;
        p = b->points[1-bi];
        b = b->neighbors[1-bi][dir];
        bi = b->index_of(p);
        bp = b->points[1-bi];
        return a;
    }
    return 0;       // NULL
}
edge* tx_side::move(point* o) {
    if (b != 0 && bp != o && !extern_angle(bp, p, o, 1-dir)) {
        edge* a = b;
        p = b->points[1-bi];
        b = b->neighbors[1-bi][dir];
        bi = b->index_of(p);
        bp = b->points[1-bi];
        return a;
    }
    return 0;      // NULL
}

// Move one edge along a convex hull (possibly with an external edge
// attached at /this/), provided we stay in sight of point o and don't get
// too close to the next seam.  Return edge we moved across, or null if we
// didn't move.
//
edge* pv_side::conditional_move(point* o, int seam) {
    if (b != 0
            && bp != o  // only adjacent edge is the external one
            && closest_seam(o) == seam
            && closest_seam(p) == seam
            && closest_seam(bp) == seam
            && !extern_angle(bp, p, o, 1-dir)) {
        a = b;
        ai = 1-bi;
        ap = p;
        p = bp;
        b = b->neighbors[1-bi][dir];
        bi = b->index_of(p);
        bp = b->points[1-bi];
        return a;
    }
    return 0;       // NULL
}

// We're stitching up a seam.  'This' captures one side of
// current base edge (not explicitly needed).  Edge bottom is at
// the "bottom" (initial end) of the seam; tells us if we cycle all
// the way around.  Point o is at other end of base.  Find candidate
// endpoint for new edge on this side, moving "up" the seam.
// Break edges as necessary.
//
point* pv_side::find_candidate(edge* bottom, point* o) {
    if (a == bottom) {
        // no more candidates on this side
        return 0;
    }
    point* c = a->points[1-ai];
    if (extern_angle(o, p, c, 1-dir)) {
        // no more candidates
        return 0;
    }
    while (true) {
        edge* na = a->neighbors[ai][1-dir];
            // next edge into region
        if (na == bottom) {
            // wrapped all the way around
            return c;
        }
        int nai = na->index_of(p);
        point* nc = na->points[1-nai];
            // next potential candidate
        if (encircled(o, c, p, nc, 1-dir)) {
            // have to break an edge
            a->destroy();
            a = na;
            ai = nai;
            c = nc;
        } else return c;
    }
}
