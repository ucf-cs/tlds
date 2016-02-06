# ------------------------------------------------------------------*- CMake -*-
# file: rstm/UserConfig.cmake
# ------------------------------------------------------------------------------
# 
#  Copyright (c) 2011
#  University of Rochester
#  Department of Computer Science
#  All rights reserved.
#
#  Copyright (c) 2009, 2010
#  Lehigh University
#  Computer Science and Engineering Department
#  All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
# 
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
# 
#     * Neither the name of the University of Rochester nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
# 
# 
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
# ------------------------------------------------------------------------------

## There are a lot of questions we need to ask the user in order to configure
## the library correctly.  Broadly, these questions fall into three categories.

## First, there are configuration options that have to do with understanding
## overheads.  As an example, most architectures support both __thread and
## pthread_getspecific.  __thread is always faster, but to understand the
## cost savings of __thread, it might be useful to use pthread_getspecific
## and measure the performance degradation

## Second, there are options relating to research features.  As an example,
## we are exploring the use of machine learning for adaptivity decisions.  We
## can make decisions at various points in program execution, and the "best"
## set of points is not yet clear, so we allow the user to specify which set
## is interesting

## Third, there are options that are relevant to how cmake does configuration,
## but are irrelevant to libstm itself. These include things like "what
## algorithms to test."

## Helper macro for managing enumerated config options. The macro defines a
## variable, sets the strings property, and checks to make sure the user
## enters valid responses. The list of enumeration values is passed as the
## ${ARGN} list.
macro (libstm_enum name default string)
  string(REPLACE ";" "|" _options "${ARGN}")
  set(${name} ${default} CACHE STRING "${string}: [${_options}]")
  set_property(CACHE ${name} PROPERTY STRINGS ${ARGN})
  if (NOT ${${name}} MATCHES "(${_options})")
    message(SEND_ERROR "Unexpected ${name}: ${${name}}")
  endif()
endmacro (libstm_enum)

## Helper macro for managing an enumerated list config option. The macro defines
## a variable, sets the strings property, and checks to make sure the user
## enters valid responses. The prime difference between an enum and an enum_list
## is that the response is treated as a semicolon delimited list of values, each
## of which must match the possibilities.
##
## The macro also provides the "all" and "none" responses, that are evaluated
## and force the list.
macro (libstm_enum_list name default string)
  string(REPLACE ";" "|" _options "${ARGN}")
  set(${name} ${default} CACHE STRING "${string}: all, none, or [${_options}]")
  set_property(CACHE ${name} PROPERTY STRINGS all none ${ARGN})
  foreach (val ${${name}})
    if (NOT ${val} MATCHES "(all|none|${_options})")
      message(SEND_ERROR "Unexpected ${name} item: ${val}")
    endif()
  endforeach()
  list(FIND ${name} all saw_all)
  list(FIND ${name} none saw_none)
  if (NOT saw_all EQUAL -1 AND NOT saw_none EQUAL -1)
    message(SEND_ERROR "${name} contains both 'all' and 'none': ${${name}}")
    break ()
  endif ()
  if (NOT saw_all EQUAL -1)
    set(${name} "${ARGN}" CACHE STRING "${string}: all, none, or [${_options}]"
      FORCE)
  endif ()
  if (NOT saw_none EQUAL -1)
    set(${name} "" CACHE STRING "${string}: all, none, or [${_options}]"
      FORCE)
  endif ()
endmacro (libstm_enum_list)


## Overhead: __thread is the default, but we allow the user to select
##          pthread_getspecific instead.  Automatically turned ON for Darwin,
##          which lacks true __thread support
cmake_dependent_option(
  libstm_use_pthread_tls
  "ON for pthread(get|set)_specific TLS; OFF (DEFAULT) for builtin." OFF
  "NOT CMAKE_SYSTEM_NAME MATCHES Darwin" ON)
mark_as_advanced(libstm_use_pthread_tls)

## Experimental: specify when adaptivity decisions are made
##               (begin/abort/commit, begin/abort, none)
libstm_enum(
  libstm_adaptation_points all
  "The instrumentation points that attempt to adapt automatically"
  all;none;begin-abort)

## Experimental: when implementing a new algorithm or CM policy, it is useful
##               to have an abort histogram that shows how many 'toxic'
##               transactions occur
option(
  libstm_enable_abort_histogram
  "ON enables a histogram of consecutive aborts" OFF)
#mark_as_advanced(libstm_enable_abort_histogram)

## Overhead: The C++ TM Draft Standard requires byte-level granularity of
##           instrumentation since tx/nontx accesses to adjacent bytes are
##           allowed.  This is forced on when building the shim, and usually
##           off otherwise.
##
##       NB: We currently force this off for big-endian platforms because it
##           relies on unions and we have not tested its correctness on non-x86
##           platforms. We put the include and test right here since it's not
##           used elsewhere yet.
include (TestBigEndian)
test_big_endian(STM_BIG_ENDIAN)
cmake_dependent_option(
  libstm_enable_byte_logging
  "ON to enable byte granularity logging (DEFAULT word logging)" OFF
  "NOT rstm_enable_itm2stm" ON)
mark_as_advanced(libstm_enable_byte_logging)

## Overhead: When we are byte logging we have the option to eliminate NOrec's
##           byte-level false conflicts byt storing the byte mask in the read
##           set. This has space overhead, as well as a bit of time overhead
##           during validation and logging. If this is enabled then NOrec can't
##           distinguish between tErue and false subword conflicts.
##
##           This option is only relevant when using byte logging. When we're
##           usign word logging the needed mask infrastructure to do subword
##           conflict detection is not available, so we turn it on.
cmake_dependent_option(
  libstm_enable_norec_false_conflicts
  "ON reduces read-log space overheads but permits byte-level false conflicts"
  OFF
  "libstm_enable_byte_logging" ON)
mark_as_advanced(libstm_enable_norec_false_conflicts)

## Overhead: A transactional compiler might use transactions to write to the
##           stack.  It is incorrect for undo/redo operations to actually
##           modify stack addresses (we could overwrite the return address of
##           the commit() function!), so the shim must have stack protection
##           turned on.
cmake_dependent_option(
  libstm_enable_stack_protection
  "ON to protect the stack during rollback/commit" OFF
  "NOT rstm_enable_itm2stm" ON)
mark_as_advanced(libstm_enable_stack_protection)

## Overhead: The C++ TM Draft Standard says that an exception that is thrown
##           using a cancel-and-throw construct should retain its values
##           after the transaction aborts.  Support for such behavior is on
##           when building the shim, and off otherwise.
cmake_dependent_option(
  libstm_enable_cancel_and_throw
  "ON to enable writes to exception objects for abort-on-throw" OFF
  "NOT rstm_enable_itm2stm" ON)
mark_as_advanced(libstm_enable_cancel_and_throw)

## Overhead: The use of SSE instructions is on for x86, but can be turned
##           off.  This also forces SSE support off for sparc.
cmake_dependent_option(
  libstm_use_sse
  "ON to use SSE for things like bit-filter intersections" ON
  "NOT CMAKE_BUILD_TYPE STREQUAL Debug AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES sparc" OFF)
