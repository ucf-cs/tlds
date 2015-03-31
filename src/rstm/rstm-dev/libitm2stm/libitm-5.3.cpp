/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

// 5.3  Error reporting

#include <cstdio>
#include <cstdlib>
#include "libitm.h"

void
_ITM_error(const _ITM_srcLocation* src, int errorCode) {
    fprintf(stderr,
            "_ITM_ encountered error code: %i with source location %s\n",
            errorCode, src->psource);
    exit(1);
}
