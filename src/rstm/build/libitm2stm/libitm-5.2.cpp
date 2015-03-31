/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

// 5.2  Version checking

#include "libitm.h"

int
_ITM_versionCompatible(int i) {
    return (i <= _ITM_VERSION_NO);
}

const char*
_ITM_libraryVersion(void) {
    return _ITM_VERSION;
}
