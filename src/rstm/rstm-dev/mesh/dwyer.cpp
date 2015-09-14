/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  dwyer.cc
 *
 *  Sequential solver
 */

#include <limits.h>     // for INT_MIN, INT_MAX
#include "common.hpp"
#include "point.hpp"
#include "edge.hpp"
#include "side.hpp"
#include "dwyer.hpp"

// Stitch up a Dwyer seam.  Points lp and rp are the extreme points
// of the  regions on either side of the seam.  Direction dir
// (ccw or cw) is the direction to move around the left region;
// 1-dir is the direction to move around the right region.
//
static void stitch(point* const lp, point* const rp, const int dir) {

    // Create side structures that capture information about
    // edges above and below lp and rp:
    //
    pv_side left;  left.initialize(lp, 1-dir);
    pv_side right;  right.initialize(rp, dir);

    // Slide down to the bottom of the seam:
    do {
    } while (left.move(right.p) != 0
                || right.move(left.p) != 0);

    // create bottom edge:
    //
    edge* ep = new edge(left.p, right.p,
                        left.a == 0 ? left.b : left.a,
                        right.a == 0 ? right.b : right.a, 1-dir);
    edge* bottom = ep;
    if (left.a == 0) left.a = bottom;
        // left region is a singleton
    if (right.a == 0) right.a = bottom;
        // right region is a singleton
    // edge* base = bottom;

    // Work up the seam creating new edges and deleting old
    // edges where necessary.  Note that {left,right}.{b,bi,bp}
    // are no longer needed.
    //
    while (1) {
        point* lc = left.find_candidate(bottom, right.p);
        point* rc = right.find_candidate(bottom, left.p);

        if (lc == 0 && rc == 0) {
            // no more candidates
            break;
        }
        // Choose between candidates:
        if (lc != 0 && rc != 0 &&
                encircled (right.p, lc, left.p, rc, dir)) {
            // Left candidate won't work; circumcircle contains
            // right candidate.
            lc = 0;
        }
        // Now we know one candidate is null and the other is not.
        if (lc == 0) {
            // use right candidate
            right.a = right.a->neighbors[1-right.ai][1-dir];
            right.ai = right.a->index_of(rc);
            right.ap = right.a->points[1-right.ai];
            right.p = rc;
            (void) new edge(left.p, rc, left.a, right.a, 1-dir);
            // base = ep;
        } else {
            // use left candidate
            left.a = left.a->neighbors[1-left.ai][dir];
            left.ai = left.a->index_of(lc);
            left.ap = left.a->points[1-left.ai];
            left.p = lc;
            (void) new edge(lc, right.p, left.a, right.a, 1-dir);
            // base = ep;
        }
    }
}

// Recursively triangulate my_points[l..r].
// Dim0 values range from [low0..high0].
// Dim1 values range from [low1..high1].
//
// Base case when 1, 2, or 3 points.
//
// Using a slight variation on Dwyer's algorithm, we partition along
// whichever dimension appears to be the widest.  For simplicity, we
// use the range of coordinate values to estimate this, which will
// be fine for uniformly distributed points.  The purpose of the
// choice is to avoid creating long edges that are likely to be
// broken when stitching subproblems back together.  We partition
// along dimension 0; parity specifies whether this is X or Y.
//
void dwyer_solve(point my_points[], const int l, const int r,
                 int low0, int high0, int low1, int high1,
                 int parity) {

    int dim0;  // int dim1;
    int dir0;  int dir1;

    // Following expression won't overflow given constraints on choice
    // of coordinates.
    if ((high0 - low0) < (high1 - low1)) {
        // dim1 has a larger value range;
        // reverse parity to swap dimensions
        int t = low0;  low0 = low1;  low1 = t;
        t = high0;  high0 = high1;  high1 = t;
        parity = 1 - parity;
    }

    if (parity == 0) {
        dim0 = xdim;  // dim1 = ydim;
        dir0 = ccw;   dir1 = cw;
    } else {
        dim0 = ydim;  // dim1 = xdim;
        dir0 = cw;    dir1 = ccw;
    }

    if (l == r) {
        return;
    }
    if (l == r-1) {
        (void) new edge(&my_points[l], &my_points[r], 0, 0, dir1);
            // direction doesn't matter in this case
        return;
    }
    if (l == r-2) {     // make single triangle
        edge* s_e2 = new edge(&my_points[l+1], &my_points[r], 0, 0, dir1);
        edge* e2 = s_e2;
        edge* s_e1 = new edge(&my_points[l], &my_points[l+1], 0, e2, dir1);
        edge* e1 = s_e1;
        if (extern_angle(&my_points[l], &my_points[l+1], &my_points[r], dir0)) {
            // new edge is dir0 of edge 1, dir1 of edge 2
            (void) new edge(&my_points[l], &my_points[r], e1, e2, dir0);
        } else {
            // new edge is dir1 of edge 1, dir0 of edge 2
            (void) new edge(&my_points[l], &my_points[r], e1, e2, dir1);
        }
        return;
    }

    // At this point we know we're not a base case; have to subdivide.

    int mid = low0/2 + high0/2;
    int i = l;  int j = r;

    while (true) {
        // invariants: [i..j] are unexamined;
        // [l..i) are all <= mid; (j..r] are all > mid.

        int i0 = 0;  int j0 = 0;

        while (i < j) {
            i0 = my_points[i].coord[dim0];
            if (i0 > mid)       // belongs in right half
                break;
            i++;
        }

        while (i < j) {
            j0 = my_points[j].coord[dim0];
            if (j0 <= mid)      // belongs in left half
                break;
            j--;
        }

        // at this point either i == j == only unexamined element
        // or i < j (found elements that need to be swapped)
        // or i = j+1 (and all elements are in order)
        if (i == j) {
            i0 = my_points[i].coord[dim0];
            if (i0 > mid) {
                // give border element to right half
                i--;
            } else {
                // give border element to left half
                j++;
            }
            break;
        }
        if (i > j) {
            i--;  j++;  break;
        }
        {   // swap:
            point t = my_points[i];
            my_points[i] = my_points[j];
            my_points[j] = t;
        }
        i++;  j--;
    }
    // Now [l..i] is the left partition and [j..r] is the right.
    // Either partition may be empty.

    if (i < l) {
        // empty left half
        dwyer_solve(my_points, j, r, mid, high0, low1, high1, parity);
    } else if (j > r) {
        // empty right half
        dwyer_solve(my_points, l, i, low0, mid, low1, high1, parity);
    } else {
        // divide and conquer
        dwyer_solve(my_points, l, i, low0, mid, low1, high1, parity);
        dwyer_solve(my_points, j, r, mid, high0, low1, high1, parity);

        // find my extreme points (have to do this _after_ recursive calls):
        //
        point* lp = 0;                  // rightmost point in left half;
        int lp0 = INT_MIN;              // X coord of lp
        for (int k = l; k <= i; k++) {
            int xv = my_points[k].coord[dim0];
            if (xv > lp0) {
                lp0 = xv;  lp = &my_points[k];
            }
        }
        point* rp = 0;                  // leftmost point in right half;
        int rp0 = INT_MAX;              // X coord of rp
        for (int k = j; k <= r; k++) {
            int xv = my_points[k].coord[dim0];
            if (xv < rp0) {
                rp0 = xv;  rp = &my_points[k];
            }
        }

        stitch(lp, rp, dir0);
    }
}
