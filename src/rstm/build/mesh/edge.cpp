/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  edge.cpp
 *
 *  Edges encapsulate the bulk of the information about the triangulation.
 *  Each edge contains references to its endpoints and to the next
 *  edges clockwise and counterclockwise about those endpoints.
 */

#include "edge.hpp"
#include "edge_set.hpp"

// Edge constructor: connect points A and B, inserting dir (CW or CCW)
// of edge Ea at the A end and 1-dir of edge Eb at the B end.
// Either or both of Ea and Eb may be null.
//
edge::edge(point* a, point* b, edge* ea, edge* eb, int dir) {
    deleted = false;
    points[0] = a;
    points[1] = b;
    initialize_end(a, ea, 0, dir);
    initialize_end(b, eb, 1, 1-dir);
    edges->insert(this);
}

// Edge destructor: take self out of edges, point edge lists, but do not
// delete /this/.  Should only be called when flipping an edge, so
// destroyed edge should have neighbors at both ends.  In FGL version,
// caller should hold locks that cover endpoints and neighbors.
//
void edge::destroy() {
    deleted = true;
    edges->erase(this);
    for (int i = 0; i < 2; i++) {
        point* p = points[i];
        edge* cw_nbor = neighbors[i][cw];
        edge* ccw_nbor = neighbors[i][ccw];
        int cw_index = cw_nbor->index_of(p);
        int ccw_index = ccw_nbor->index_of(p);
        cw_nbor->neighbors[cw_index][ccw] = ccw_nbor;
        ccw_nbor->neighbors[ccw_index][cw] = cw_nbor;
        if (p->first_edge == this)
            p->first_edge = ccw_nbor;
    }
}

// Edge may not be Delaunay.  See if it's the
// diagonal of a convex quadrilateral.  If so, check
// whether it should be flipped.  If so, return the     //    c     |
// edges of the quadrilateral for future reconsidera-   //   / \    |
// tion.  If tnal is false, do all this only if all     //  a - b   |
// the points I touch are closest to my seam: other-    //   \ /    |
// wise, if called nontransactionally, return false     //    d     |
// so caller knows to reconsider edge in a future
// synchronized phase.
//
bool edge::reconsider(const int seam, bool txnal,
                      edge** surrounding_edges) {
    if (deleted) {
        // This edge has already been reconsidered, and deleted.
        for (int i = 0; i < 4; i++) surrounding_edges[i] = 0;
        return true;
    }
    point* a = points[0];
    assert (txnal || closest_seam(a) == seam);
    point* b = points[1];
    assert (txnal || closest_seam(b) == seam);
    edge* ac = neighbors[0][ccw];
    edge* bc = neighbors[1][cw];
    point* c = ac->points[1-ac->index_of(a)];
    // a and b are assumed to be closest to my seam.
    // I have to check c and d.
    if (!txnal && closest_seam(c) != seam) {
        // I can't safely consider this flip in this phase of
        // the algorithm.  Defer to synchronized phase.
        return false;
    }
    if (c != bc->points[1-bc->index_of(b)]) {
        // No triangle on the c side; we're an external edge
        for (int i = 0; i < 4; i++) surrounding_edges[i] = 0;
        return true;
    }
    edge* ad = neighbors[0][cw];
    edge* bd = neighbors[1][ccw];
    point* d = ad->points[1-ad->index_of(a)];
    if (!txnal && closest_seam(d) != seam) {
        // I can't safely consider this flip in this phase of
        // the algorithm.  Defer to synchronized phase.
        return false;
    }
    if (d != bd->points[1-bd->index_of(b)]) {
        // No triangle on the d side; we're an external edge
        for (int i = 0; i < 4; i++) surrounding_edges[i] = 0;
        return true;
    }
    if (encircled(b, c, a, d, ccw) || encircled(a, d, b, c, ccw)) {
        // other diagonal is locally Delaunay; we're not
        destroy();          // have to do this NOW, before creating cross edge
        (void) new edge(c, d, bc, bd, cw);
        surrounding_edges[0] = ac;
        surrounding_edges[1] = ad;
        surrounding_edges[2] = bc;
        surrounding_edges[3] = bd;
        return true;
    }
    for (int i = 0; i < 4; i++) surrounding_edges[i] = 0;
    return true;
}

#ifdef FGL
// Alternative, FGL version: optimistic fine-grain locking.
// Identifies the relevant edges and points, then locks
// them in canonical order, then double-checks to make
// sure nothing has changed.
//
void edge::synchronized_reconsider(const int seam,
                             simple_queue<edge*> *tentative_edges) {
    point* a = points[0];
    point* b = points[1];
    edge* ac = neighbors[0][ccw];
    edge* bc = neighbors[1][cw];
    int aci = ac->index_of(a);
    int bci = bc->index_of(b);
    if (aci == -1 || bci == -1) {
        // inconsistent; somebody has already been here
        return;
    }
    point* c = ac->points[1-aci];
    if (c != bc->points[1-bci]) {
        // No triangle on the c side; we're an external edge
        return;
    }
    edge* ad = neighbors[0][cw];
    edge* bd = neighbors[1][ccw];
    int adi = ad->index_of(a);
    int bdi = bd->index_of(b);
    if (adi == -1 || bdi == -1) {
        // inconsistent; somebody has already been here
        return;
    }
    point* d = ad->points[1-adi];
    if (d != bd->points[1-bdi]) {
        // No triangle on the d side; we're an external edge
        return;
    }
    {
        point_set S;
        with_locked_points cs(S | a | b | c | d);

        if (!(edges->contains(this)
                && edges->contains(ac) && edges->contains(bc)
                && edges->contains(ad) && edges->contains(bd))) {
            // inconsistent; somebody has already been here
            return;
        }
        if (!(ac == neighbors[0][ccw]
                && bc == neighbors[1][cw]
                && ad == neighbors[0][cw]
                && bd == neighbors[1][ccw])) {
            // inconsistent; somebody has already been here
            return;
        }
        if (!(aci == ac->index_of(a)
                && bci == bc->index_of(b)
                && adi == ad->index_of(a)
                && bdi == bd->index_of(b))) {
            // inconsistent; somebody has already been here
            return;
        }
        if (!(c == ac->points[1-aci]
                && c == bc->points[1-bci]
                && d == ad->points[1-adi]
                && d == bd->points[1-bdi])) {
            // inconsistent; somebody has already been here
            return;
        }
        if (encircled(b, c, a, d, ccw) || encircled(a, d, b, c, ccw)) {
            // other diagonal is locally Delaunay; we're not
            destroy();          // have to do this NOW, before creating cross edge
            (void) new edge(c, d, bc, bd, cw);
            tentative_edges->enqueue(ac, seam);
            tentative_edges->enqueue(ad, seam);
            tentative_edges->enqueue(bc, seam);
            tentative_edges->enqueue(bd, seam);
        }
    }
}
#endif  // FGL
