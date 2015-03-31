/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  edge.hpp
 *
 *  Edges encapsulate the bulk of the information about the triangulation.
 *  Each edge contains references to its endpoints and to the next
 *  edges clockwise and counterclockwise about those endpoints.
 */

#ifndef EDGE_HPP__
#define EDGE_HPP__

#include <iostream>
    using std::ostream;
    using std::cout;
    using std::cerr;
#include <sstream>
    using std::stringstream;
#include <string>
    using std::string;

#include "queues.hpp"
#include "common.hpp"
#include "point.hpp"
#include "my_thread.hpp"
#include "lock.hpp"

class edge {

    // Utility routine for constructor.
    //
    TRANSACTION_SAFE
    void initialize_end(point* p, edge* e, int end, int dir) {
        if (e == 0) {
            neighbors[end][dir] = this;
            neighbors[end][1-dir] = this;
            p->first_edge = this;
        } else {
            int i = e->index_of(p);
            neighbors[end][1-dir] = e;
            neighbors[end][dir] = e->neighbors[i][dir];
            e->neighbors[i][dir] = this;
            edge* nbor = neighbors[end][dir];
            i = nbor->index_of(p);
            nbor->neighbors[i][1-dir] = this;
        }
    }

public:
    point* points[2];
    edge* neighbors[2][2];
        // indexed first by edge end and then by direction
    bool deleted;
        // Has this edge been deleted from the mesh because it isn't Delaunay?

    // print self in canonical form (leftmost point first)
    // send to cout if not in transaction; else to currentThread->vout
    //
    TRANSACTION_PURE void print(const char* prefix = "") const {
        point* a = points[0];
        point* b = points[1];
        if (*b < *a) {
            point* t = a;  a = b;  b = t;
        }
        stringstream ss;
        ss << prefix
           << a->coord[xdim] << " " << a->coord[ydim] << " "
           << b->coord[xdim] << " " << b->coord[ydim] << "\n";
        if (currentThread->in_transaction()) {
            currentThread->vout << ss.str();
        } else {
            with_lock cs(io_lock);
            cout << ss.str();
        }
    }

    // Return index of point p within edge, or (in FGL version) -1 if not
    // found.
    //
    TRANSACTION_SAFE int index_of(const point* p) {
        if (points[0] == p) return 0;
        if (points[1] == p) return 1;
#ifndef FGL
        assert(false);
#endif
        return -1;
    }

    // Edge may not be Delaunay.  See if it's the diagonal of a convex
    // quadrilateral.  If so, check whether it should be flipped.
    // If so, queue the edges of the quadrilateral for future
    // reconsideration.  Do all of this only if all points I touch
    // are closest to my seam.  If I don't own what I need to, return
    // false, so caller knows to queue /this/ for future, synchronized
    // reconsideration.  If I _do_ own what I need to, return edges of
    // quadrilateral for reconsideration.
    //
    TRANSACTION_SAFE
    bool reconsider(const int seam, bool txnal,
                    edge** surrounding_edges);

#ifdef FGL
    // Like regular reconsider above, but optimistically fine-grain
    // synchronized: identifies the relevant edges and points, then
    // locks them in canonical order, then double-checks to make
    // sure nothing has changed.
    //
    void synchronized_reconsider(const int seam,
                                 simple_queue<edge*> *tentative_edges);
#endif  // FGL

    // Edge constructor and destructor are separately compiled to avoid a
    // circular dependence on edge_set.hpp.

    // Edge constructor: connect points A and B, inserting dir (CW or CCW)
    // of edge Ea at the A end and 1-dir of edge Eb at the B end.
    // Either or both of Ea and Eb may be null.
    //
    TRANSACTION_SAFE
    edge(point* a, point* b, edge* ea, edge* eb, int dir);

    // Edge removal: take self out of edges, point edge lists, but do not
    // delete /this/.  Should only be called when flipping an edge, so
    // destroyed edge should have neighbors at both ends.  In FGL version,
    // caller should hold locks that cover endpoints and neighbors.
    //
    TRANSACTION_SAFE
    void destroy();

    // Edge objects must never be deleted: they may be looked at after
    // some other thread has deleted them.
    //
    ~edge() {assert(false);}        // Should never be called.
};

#endif // EDGE_HPP__
