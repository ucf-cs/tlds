/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  worker.cpp
 *
 *  Main file for the parallel solver.
 */

#include <limits.h>     // for INT_MIN, INT_MAX

#include "common.hpp"
#include "point.hpp"
#include "edge.hpp"
#include "side.hpp"
#include "lock.hpp"
#include "barrier.hpp"
#include "dwyer.hpp"
#include "edge_set.hpp"
#include "my_thread.hpp"
#include "worker.hpp"

// Do quick stitch-up of the guaranteed-private portion
// of a seam between regions.  Returns edge at which it
// stopped.
//
static edge* baste(pv_side &left, pv_side &right,
            const int dir, const int col,
            edge* starter, simple_queue<edge*>* tentative_edges)
{
    edge* cur_edge = starter;
    while (1) {
        int ly = left.p->coord[ydim];
        int ry = right.p->coord[ydim];
        edge* traversed;
        if (dir == ccw ? (ly > ry) : (ly < ry)) {
            // prefer to move on the left
            traversed = left.conditional_move(right.p, col);
            if (traversed == 0) {
                traversed = right.conditional_move(left.p, col);
            }
        } else {
            // prefer to move on the right
            traversed = right.conditional_move(left.p, col);
            if (traversed == 0) {
                traversed = left.conditional_move(right.p, col);
            }
        }
        if (traversed == 0) break;
        edge* ep = new edge(left.p, right.p, left.b, right.b, dir);
        cur_edge = ep;
        tentative_edges->enqueue(ep, col);
        tentative_edges->enqueue(traversed, col);
    }
    return cur_edge;
}


template <typename T>
static TRANSACTION_PURE
void txasserteq(T lhs, T rhs) {
    assert(lhs == rhs);
}


// Similar to regular baster, but for the portion of the
// seam beyond the guaranteed-to-be-private part.
//
static void synchronized_baste(point* lp, point* rp, int dir,
        const int col, edge* cur_edge, region_info **regions)
#ifndef FGL
{
    bool done = false;
    while (!done) {
        edge* traversed = NULL;     // initialize to avoid complaint from gcc

        BEGIN_TRANSACTION(atomic) {
            tx_side left;
            tx_side right;

            // Workaround for icc redo-log, RAW stack location bug.
            point** volatile workaround_left_p = &left.p;
            point** volatile workaround_right_p = &right.p;
            edge** volatile workaround_left_b = &left.b;
            edge** volatile workaround_right_b = &right.b;
            point** volatile workaround_left_bp = &left.bp;
            point** volatile workaround_right_bp = &right.bp;

            done = false;
            left.initialize(cur_edge, lp, 1-dir);
            right.initialize(cur_edge, rp, dir);
            if (*workaround_left_bp == *workaround_right_bp) {
                // I've been boxed in by a neighbor
                done = true;
            } else {
                int ly = (*workaround_left_p)->coord[ydim];
                int ry = (*workaround_right_p)->coord[ydim];
                if (dir == ccw ? (ly > ry) : (ly < ry)) {
                    // prefer to move on the left
                    traversed = left.move(*workaround_right_p);
                    if (traversed == NULL) {
                        traversed = right.move(*workaround_left_p);
                    }
                } else {
                    // prefer to move on the right
                    traversed = right.move(*workaround_left_p);
                    if (traversed == NULL) {
                        traversed = left.move(*workaround_right_p);
                    }
                }
                if (traversed == NULL) {
                    // can't move
                    done = true;
                } else {
                    cur_edge = new edge(*workaround_left_p,
                                        *workaround_right_p,
                                        *workaround_left_b,
                                        *workaround_right_b,
                                        dir);
                }
            }
            if (!done) {
                lp = *workaround_left_p;
                rp = *workaround_right_p;
            }
        } END_TRANSACTION;

        if (done) return;
        // Enqueues should be done only if the transaction commits:
        regions[col]->tentative_edges->enqueue(cur_edge, col);
        if (traversed != 0) {
            regions[col]->tentative_edges->enqueue(traversed, col);
        }
    }
}

#else  // FGL

{
    class inconsistency {};     // exception
    edge* cur = cur_edge;
    while (1) {  // retry if inconsistency encountered
        try {
            while (1) {
                pv_side left;   left.reinitialize(cur, lp, 1-dir);
                pv_side right;  right.reinitialize(cur, rp, dir);
                if (left.bp == right.bp) {
                    // I've been boxed in by a neighbor
                    return;
                }
                int ly = left.p->coord[ydim];
                int ry = right.p->coord[ydim];
                edge* traversed = 0;
                if (dir == ccw ? (ly > ry) : (ly < ry)) {
                    // prefer to move on the left
                    traversed = left.move(right.p);
                    if (traversed == 0) {
                        traversed = right.move(left.p);
                    }
                } else {
                    // prefer to move on the right
                    traversed = right.move(left.p);
                    if (traversed == 0) {
                        traversed = left.move(right.p);
                    }
                }
                if (traversed == 0) return;     // can't move
                {
                    point_set S;
                    with_locked_points cs(S | lp | rp | left.p | right.p);
                        // two of those will be the same, but that's ok
                    if (left.a->neighbors[left.ai][1-dir] != left.b
                            || right.a->neighbors[right.ai][dir] != right.b
                            || left.a->neighbors[1-left.ai][dir] != right.a) {
                        // inconsistent; somebody else has been here
                        throw inconsistency();
                    }
                    cur_edge = new edge(left.p, right.p, left.b, right.b, dir);
                    cur = cur_edge;
                    regions[col]->tentative_edges->enqueue(cur_edge, col);
                    regions[col]->tentative_edges->enqueue(traversed, col);
                }
                lp = left.p;
                rp = right.p;
            }
        } catch (inconsistency) {}
    }
}
#endif  // FGL

//  Utility routine to avoid typing the body 3 times.
//  To "reconsider" an edge is to check and make sure it's Delaunay, and,
//  if not, to replace it with its flip and nominate the four surrounding
//  edges for future reconsideration.
//
static void reconsider_edge(edge* e, const int col, region_info **regions) {
#ifdef FGL
    e->synchronized_reconsider(col, regions[col]->tentative_edges);
#else
    edge* surrounding_edges[4];
    bool handled;
    BEGIN_TRANSACTION(atomic) {
        handled = e->reconsider(col, true, surrounding_edges);
    } END_TRANSACTION;
    assert(handled);    // should never return false when transactional
    for (int i = 0; i < 4; i++) {
        edge *e = surrounding_edges[i];
        if (e) {
            regions[col]->tentative_edges->enqueue(e, col);
        }
    }
#endif
}

//  Main routine for workers.
//  Called from runnable worker.
//  Assume num_workers > 1.
//
static void do_work(const int col, region_info **regions, barrier *bar)
{
    static point* sorted_points;    // Shared temporary array.  Does not
                                    // need to be synchronized (see algorithm).
    int my_start = 0;               // Index in sorted_points

    // Figure out how many of each of my peers' points I have.
    // No synchronization required.

    regions[col] = new region_info(col);

    for (int i = 0; i < num_workers; i++) {
        regions[col]->counts[i] = 0;
    }
    for (int i = (num_points * col) / num_workers;
             i < (num_points * (col+1)) / num_workers; i++) {
        regions[col]->counts[stripe(&all_points[i])]++;
    }
    if (col == 0) {
        sorted_points = new point[num_points];
    }
    edges->help_initialize(col);

    bar->wait("");

    // Give appropriate points to peers.
    // No synchronization required.

    int peer_start[num_workers];
    int cursor = 0;
    // for each region:
    for (int i = 0; i < num_workers; i++) {
        if (i == col) {
            my_start = cursor;
        }
        // for all peers to the left:
        for (int j = 0; j < col; j++) {
            cursor += regions[j]->counts[i];
        }
        peer_start[i] = cursor;
            // beginning of points in region i supplied by me
        // for all peers to the right:
        for (int j = col; j < num_workers; j++) {
            cursor += regions[j]->counts[i];
        }
        if (i == col) {
            regions[col]->npts = cursor - my_start;
        }
    }

    for (int i = (num_points * col) / num_workers;
             i < (num_points * (col+1)) / num_workers; i++) {
        sorted_points[peer_start[stripe(&all_points[i])]++]
            = all_points[i];
    }

    bar->wait("");

    if (col == 0) {
        delete[] all_points;
        all_points = sorted_points;
    }

    bar->wait("point partitioning");

    // Triangulate my region (vertical stripe).
    // No synchronization required.

    // Find my extreme values:
    int miny = -(2 << (MAX_COORD_BITS-1));      // close enough
    int maxy = (2 << (MAX_COORD_BITS-1)) - 1;   // close enough
    int minx = INT_MAX;
    int maxx = INT_MIN;
    int t;
    for (int i = my_start; i < my_start + regions[col]->npts; i++) {
        if ((t = all_points[i].coord[xdim]) < minx) minx = t;
        if ((t = all_points[i].coord[xdim]) > maxx) maxx = t;
    }

    if (regions[col]->npts > 0) {
        dwyer_solve(all_points,
                    my_start, my_start + regions[col]->npts - 1,
                    minx, maxx, miny, maxy, 0);
    }

    bar->wait("Dwyer triangulation");

    // Find my extreme points (have to do this _after_ dwyer_solve;
    // it moves points):
    for (int i = my_start; i < my_start + regions[col]->npts; i++) {
        if (all_points[i].coord[xdim] == minx) {
            regions[col]->leftmost = &all_points[i];
        }
        if (all_points[i].coord[xdim] == maxx) {
            regions[col]->rightmost = &all_points[i];
        }
    }

    bar->wait("");

    int next_col = col + 1;
    while (next_col < num_workers &&
            regions[next_col]->npts == 0) {
        ++next_col;
    }
    int neighbor = next_col;
    bool have_seam = regions[col]->npts != 0 && neighbor < num_workers;

    // create initial edge between regions.
    // Must be synchronized to accommodate singleton regions.

    edge* starter = 0;

    if (have_seam) {
        // Connect rightmost point in my region with leftmost
        // point in the region to my right.
        point* lp = regions[col]->rightmost;
        point* rp = regions[neighbor]->leftmost;
#ifdef FGL
        {   point_set S;
            with_locked_points cs(S | lp | rp);
            edge* lb = hull_edge(lp, cw);
            edge* rb = hull_edge(rp, ccw);
            starter = new edge(lp, rp, lb, rb, ccw);
        }
#else
        BEGIN_TRANSACTION(atomic) {
            edge* lb = hull_edge(lp, cw);
            edge* rb = hull_edge(rp, ccw);
            starter = new edge(lp, rp, lb, rb, ccw);
        } END_TRANSACTION;
#endif
    }

    bar->wait("initial cross edges");

    edge* cur_edge = 0;
    pv_side left;  pv_side right;
    sequential_queue<edge*> my_tentative_edges;

    // Stitch up the guaranteed-private lower part of
    // the joint between me and my neighbor to the right.

    if (have_seam) {
        // Work my way down the seam, so long as all points
        // I inspect are closer to my seam than to a neighboring
        // seam.  Don't worry about whether edges are
        // Delaunay or not.  Note that {left,right}.{a,ai,ap} are
        // no longer needed.

        left.reinitialize(starter, regions[col]->rightmost, cw);
        right.reinitialize(starter, regions[neighbor]->leftmost, ccw);
        if (closest_seam(left.p) == col
                && closest_seam(right.p) == col) {
            my_tentative_edges.enqueue(starter, col);
            cur_edge = baste(left, right, ccw, col,
                             starter, &my_tentative_edges);
        } else {
            // access to starter must be synchronized
            regions[col]->tentative_edges->enqueue(starter, col);
            cur_edge = starter;
        }
    }

    bar->wait("lower private baste");

    // Work down the disputed lower portion of the seam, synchronizing
    // with other threads, and quitting if one of them boxes me in.

    if (have_seam) {
        synchronized_baste(left.p, right.p, ccw, col, cur_edge, regions);
    }

    bar->wait("lower synchronized baste");

    // Stitch up the guaranteed-private upper part of
    // the joint between me and my neighbor to the right.

    if (have_seam) {
        left.p = starter->points[0];
        right.p = starter->points[1];
            // Those last two lines depend on the assumption that the
            // edge constructor makes its first argument points[0].
        left.update(starter, ccw);
        right.update(starter, cw);
        if (closest_seam(left.p) == col
                && closest_seam(right.p) == col) {
            cur_edge = baste(left, right, cw, col,
                             starter, &my_tentative_edges);
        } else {
            cur_edge = starter;
        }
    }

    bar->wait("upper private baste");

    // Work up the disputed upper portion of the seam,
    // synchronizing with other threads, and quitting if one of them
    // boxes me in.

    if (have_seam) {
        synchronized_baste(left.p, right.p, cw, col, cur_edge, regions);
    }

    bar->wait("upper synchronized baste");

    // Reconsider those edges that are guaranteed to be
    // in my geometric region (closest to my seam):

    {   edge* e;
        while ((e = my_tentative_edges.dequeue(col)) != 0) {
            edge* surrounding_edges[4];
            if (e->reconsider(col, false, surrounding_edges)) {
                for (int i = 0; i < 4; i++) {
                    edge *e = surrounding_edges[i];
                    if (e) {
                        my_tentative_edges.enqueue(e, col);
                    }
                }
            } else {
                // have to reconsider edge in a later synchronized phase
                regions[col]->tentative_edges->enqueue(e, col);
            }
        }
    }

    bar->wait("private reconsideration");

    // Reconsider edges in disputed territory:

    {   edge* e;
        while ((e = regions[col]->tentative_edges->dequeue(col)) != 0) {
            reconsider_edge(e, col, regions);
        }
        // try to help peers (simplistic work stealing):
        int i = (col + 1) % num_workers;
        while (i != col) {
            while ((e = regions[i]->tentative_edges->dequeue(col)) != 0) {
                reconsider_edge(e, col, regions);
                while ((e = regions[col]->tentative_edges->
                                                     dequeue(col)) != 0) {
                    reconsider_edge(e, col, regions);
                }
            }
            i = (i + 1) % num_workers;
        }
    }

    bar->wait("synchronized reconsideration");
}

void worker::operator()() {
    do_work(col, regions, bar);
}
