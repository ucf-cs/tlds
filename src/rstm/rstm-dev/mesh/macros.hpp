/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/*  macros.hpp
 *
 *  Macros used in the mesh app.
 */

#ifndef MACROS_HPP__
#define MACROS_HPP__

#include <iostream>
#include <string.h>     // for strerror()
#include <cassert>

// for syscalls:
//
#define VERIFY(E)                                                       \
    {                                                                   \
        int stat = (E);                                                 \
        if (stat != 0) {                                                \
         std::cerr << __FILE__ << "[" << __LINE__ << "] bad return("    \
                   << stat << "): " << strerror(stat) << "\n" << std::flush; \
         assert(false);                                                 \
        }                                                               \
    }

#endif // MACROS_HPP__
