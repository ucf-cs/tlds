/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  dwyer.h
 *
 *  Sequential solver
 */

#ifndef DWYER_HPP__
#define DWYER_HPP__

#include "point.hpp"

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
extern void dwyer_solve(point my_points[], int l, int r,
                        int low0, int high0, int low1, int high1,
                        int parity);

#endif // DWYER_HPP__
