/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/**
 *  Use the configuration settings to define the API to use in applications
 *  that support both LIBRARY and COMPILER instrumentation.
 *
 *  NB: This is not exactly the best way to be handling the api, since stamp
 *      uses default, but it works...
 */

#ifndef STM_API_HPP__
#define STM_API_HPP__

#include <stm/config.h>

#if defined(STM_API_CXXTM)
#  include <api/cxxtm.hpp>
#elif defined(STM_API_STAMP)
#  include <api/stamp.hpp>
#else // default
#  include <api/library.hpp>
#endif

#endif // STM_API_HPP__
