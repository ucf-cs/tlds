/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#ifndef POINT_HPP__
#define POINT_HPP__

#include <set>
    using std::set;
#include "common.hpp"

class edge;

static const int xdim = 0;
static const int ydim = 1;
static const int ccw = 0;       // counter-clockwise
static const int cw = 1;        // clockwise

static const int MAX_COORD_BITS = 24;
    // coordinate values in the range -2^24..(2^24-1)
    // permit bucket calculations without overflow, so long as
    // there are no more than 32 threads

class point;
extern point *all_points;

class point
{
    // Point coordinates are immutable, but their first_edge field can change.
    // In order to prevent bugs, we disable copying and assignment.

#ifdef FGL
    d_lock l;
#endif

public:
    int coord[2];       // final; no accessors needed
    edge* first_edge;

    size_t hash() const
    {
        return coord[xdim] ^ coord[ydim];
    }

    bool operator==(const point &o) const
    {
        return coord[xdim] == o.coord[xdim]
            && coord[ydim] == o.coord[ydim];
    }

    bool operator!=(const point &rhs) const
    {
        return !(*this == rhs);
    }

    // For locking and printing in canonical (sorted) order.
    //
    TRANSACTION_SAFE bool operator<(const point& o) const {
        return (coord[xdim] < o.coord[xdim]
                || (coord[xdim] == o.coord[xdim]
                    && coord[ydim] < o.coord[ydim]));
    }

#ifdef FGL
    void acquire() {
        l.acquire();
    }
    void release() {
        l.release();
    }
#endif  // FGL

  point() { }     // no copying, assignment allowed
};

struct eq_point {
    bool operator()(const point* const p1, const point* const p2) const {
        return *p1 == *p2;
    }
};

struct lt_point {
    bool operator()(const point* const p1, const point* const p2) const {
        return *p1 < *p2;
    }
};

struct hash_point {
    size_t operator()(const point* const p) const {
        return p->hash();
    }
};

#ifdef FGL
// lock all points in given set at beginning of scope, release at end
//
class with_locked_points {
    set<point*, lt_point> *S;
public:
    with_locked_points(set<point*, lt_point> &pts) {
        S = &pts;
        for (set<point*, lt_point>::iterator it = S->begin();
                it != S->end(); ++it) {
            (*it)->acquire();
        }
    }
    ~with_locked_points() {
        for (set<point*, lt_point>::reverse_iterator it = S->rbegin();
                it != S->rend(); ++it) {
            (*it)->release();
        }
    }
};

// To facilitate constructions of the form
//     point_set S;
//     with_locked_points lp(S | p1 | p2 | ... | pN);
//
class point_set : public set<point*, lt_point> {
public:
    point_set &operator|(point *e) {
        insert(e);
        return *this;
    }
};
#endif  // FGL

// these are set by create_points().
//
extern int min_coord[];
extern int max_coord[];

// The point space is divided into vertical "buckets" -- twice as many as
// there are worker threads.  During initial Dwyer triangulation, thread
// zero works, privately, on buckets 0 and 1; thread 1 on buckets 2 and 3;
// etc.  During private basting, each thread takes a _seam_ and works on
// the buckets closest to it: thread 0 on buckets 0, 1, and 2; thread 1
// on buckets 3 and 4; ...; thread n-1 on buckets 2n-3, 2n-2, and 2n-1.
//
// Routines bucket, stripe, and closest_seam manage this division of labor.
// To avoid integer overflow, they count on coordinate values spanning less
// than 2^26.
//
// Global set edges contains a separate hash table for each bucket, ensuring
// private access during each private phase of computation.
//
TRANSACTION_SAFE inline int bucket(const point* const p) {
    return ((p->coord[xdim] - min_coord[xdim]) * num_workers * 2)
            / (max_coord[xdim] - min_coord[xdim] + 1);
}
inline int stripe(const point* const p) {
    return bucket(p) / 2;
}
TRANSACTION_SAFE inline int closest_seam(const point* const p) {
    const int b = bucket(p);
    if (b == 0) return 0;
    if (b == (num_workers * 2 - 1)) return num_workers - 2;
    return (b-1)/2;
}

// Does circumcircle of A, B, C (ccw) contain D?
//
TRANSACTION_SAFE bool encircled(const point* A, const point* B,
               const point* C, const point* D, const int dir);

// Is angle from p1 to p2 to p3, in direction dir
// around p2, greater than or equal to 180 degrees?
//
TRANSACTION_SAFE bool extern_angle(const point* p1, const point* p2,
                  const point* p3, const int dir);

// Create all points.  Read from stdin if seed == 0.
//
extern void create_points(const int seed);

#endif // POINT_HPP__
