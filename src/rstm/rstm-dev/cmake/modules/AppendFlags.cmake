#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

## Simple macros for adding to the CXX_FLAGS and CXXTM_FLAGS strings

## append_flags is a helper macro.  It isn't used outside of this file
macro (append_flags var)
  foreach (arg ${ARGN})
    set(${var} "${${var}} ${arg}")
  endforeach ()
endmacro ()

## This macro lets us easily add to the C_FLAGS string
macro (append_c_flags)
  append_flags(CMAKE_C_FLAGS ${ARGN})
endmacro ()

## This macro lets us easily add to the CXX_FLAGS string
macro (append_cxx_flags)
  append_flags(CMAKE_CXX_FLAGS ${ARGN})
endmacro (append_cxx_flags)

## This one lets us easily add to the CXXTM_FLAGS string
macro (append_cxxtm_flags)
  append_flags (CMAKE_CXX-tm_FLAGS ${ARGN})
endmacro (append_cxxtm_flags)
