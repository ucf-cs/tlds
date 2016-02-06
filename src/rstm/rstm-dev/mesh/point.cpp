/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  point.cpp
 *
 *  Points coordinates are immutable, but their first_edge field can change.
 */

#include <limits.h>     // for INT_MIN, INT_MAX
#include <stdlib.h>     // for random()

#include <tr1/unordered_set>
    using std::tr1::unordered_set;

#include <iostream>
    using std::cout;
    using std::cerr;
#include <iomanip>
    using std::flush;
#include "common.hpp"
#include "point.hpp"

point* all_points;

int min_coord[2] = {INT_MAX, INT_MAX};
int max_coord[2] = {INT_MIN, INT_MIN};

// 3x3 determinant
//
static TRANSACTION_SAFE double det3(
        const double a, const double b, const double c,
        const double d, const double e, const double f,
        const double g, const double h, const double i) {
    return a * (e*i - f*h)
         - b * (d*i - f*g)
         + c * (d*h - e*g);
}

// 4x4 determinant
//
static TRANSACTION_SAFE double det4(
        const double a, const double b, const double c, const double d,
        const double e, const double f, const double g, const double h,
        const double i, const double j, const double k, const double l,
        const double m, const double n, const double o, const double p) {
    return a * det3(f, g, h, j, k, l, n, o, p)
         - b * det3(e, g, h, i, k, l, m, o, p)
         + c * det3(e, f, h, i, j, l, m, n, p)
         - d * det3(e, f, g, i, j, k, m, n, o);
}

// If A, B, and C are on a circle, in counter-clockwise order, then
// D lies within that circle iff the following determinant is positive:
//
// | Ax  Ay  Ax^2+Ay^2  1 |
// | Bx  By  Bx^2+By^2  1 |
// | Cx  Cy  Cx^2+Cy^2  1 |
// | Dx  Dy  Dx^2+Dy^2  1 |
//
bool encircled(const point* A, const point* B,
               const point* C, const point* D, const int dir) {
    if (dir == cw) {
        const point* t = A;  A = C;  C = t;
    }
    double Ax = A->coord[xdim];   double Ay = A->coord[ydim];
    double Bx = B->coord[xdim];   double By = B->coord[ydim];
    double Cx = C->coord[xdim];   double Cy = C->coord[ydim];
    double Dx = D->coord[xdim];   double Dy = D->coord[ydim];

    return det4(Ax, Ay, (Ax*Ax + Ay*Ay), 1,
                Bx, By, (Bx*Bx + By*By), 1,
                Cx, Cy, (Cx*Cx + Cy*Cy), 1,
                Dx, Dy, (Dx*Dx + Dy*Dy), 1) > 0;
}

// Is angle from p1 to p2 to p3, in direction dir
// around p2, greater than or equal to 180 degrees?
//
bool extern_angle(const point* p1, const point* p2,
                  const point* p3, const int dir) {
    if (dir == cw) {
        const point* t = p1;  p1 = p3;  p3 = t;
    }
    int x1 = p1->coord[xdim];     int y1 = p1->coord[ydim];
    int x2 = p2->coord[xdim];     int y2 = p2->coord[ydim];
    int x3 = p3->coord[xdim];     int y3 = p3->coord[ydim];

    if (x1 == x2) {                     // first segment vertical
        if (y1 > y2) {                  // points down
            return (x3 >= x2);
        } else {                        // points up
            return (x3 <= x2);
        }
    } else {
        double m = (((double) y2) - y1) / (((double) x2) - x1);
            // slope of first segment
        if (x1 > x2) {      // points left
            return (y3 <= m * (((double) x3) - x1) + y1);
            // p3 below line
        } else {            // points right
            return (y3 >= m * (((double) x3) - x1) + y1);
            // p3 above line
        }
    }
}

// Create all points.  Read from stdin if seed == 0.
//
void create_points(const int seed)
{
    all_points = new point[num_points];
    unordered_set<point*, hash_point, eq_point> point_hash;

    srandom(seed);

    for (point* p = all_points; p < all_points + num_points; p++) {
        if (seed == 0) {
            int x;  scanf("%d", &x);
            int y;  scanf("%d", &y);
            assert(x >= -(2 << (MAX_COORD_BITS-1))
                        && x <= ((2 << (MAX_COORD_BITS-1)) - 1));
            assert(y >= -(2 << (MAX_COORD_BITS-1))
                        && y <= ((2 << (MAX_COORD_BITS-1)) - 1));
            p->coord[xdim] = x;
            p->coord[ydim] = y;
            assert(point_hash.find(p) == point_hash.end());
                // no duplicates allowed
        } else {
            do {
                p->coord[xdim] = (random() &
                    ((2 << (MAX_COORD_BITS))-1)) - (2 << (MAX_COORD_BITS-1));
                p->coord[ydim] = (random() &
                    ((2 << (MAX_COORD_BITS))-1)) - (2 << (MAX_COORD_BITS-1));
            } while (point_hash.find(p) != point_hash.end());
        }

        point_hash.insert(p);
        if (p->coord[xdim] < min_coord[xdim]) min_coord[xdim] = p->coord[xdim];
        if (p->coord[ydim] < min_coord[ydim]) min_coord[ydim] = p->coord[ydim];
        if (p->coord[xdim] > max_coord[xdim]) max_coord[xdim] = p->coord[xdim];
        if (p->coord[ydim] > max_coord[ydim]) max_coord[ydim] = p->coord[ydim];
    }

    // Print point ranges for benefit of optional display tool:
    if (output_incremental || output_end) {
        cout << min_coord[xdim] << " " << max_coord[xdim] << " "
             << min_coord[ydim] << " " << max_coord[ydim] << flush << "\n";
    }
}
