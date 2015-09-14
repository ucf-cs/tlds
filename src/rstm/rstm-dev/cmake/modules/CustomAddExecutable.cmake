#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

include (AppendProperty)

## We have set up RSTM so that it can be configured to independently build 32
## and 64-bit libraries and executables. The user can configure a build
## directory for neither, either, or both. We have a list, rstm_archs, that
## contains the compiler architecture we want to build for (32;64). We use that
## list in order to register multiple versions of executables (foreack
## arch). This file provides some macros that simplify this "multiarch"
## executable generation.

## Add a multiarch executable.
macro (add_multiarch_executable exec name arch)
  set(${exec} "${name}${arch}")
  add_executable(${${exec}} ${ARGN})
  append_property(TARGET ${${exec}} LINK_FLAGS -m${arch})
  append_property(TARGET ${${exec}} COMPILE_FLAGS -m${arch})
endmacro ()

## Add a multiarch executable for which some of the source files are compiled
## with the CXX-tm compiler.
macro (add_cxxtm_executable exec name arch)
  add_multiarch_executable(${exec} ${name} ${arch} ${ARGN})

  # Add the CXX-tm implicit libraries (the EXTRA_LIBS are detected at language
  # configuration time, and are _not_ a standard thing... usually the
  # IMPLICIT_LIBRARIES are used, but we choose not to remember them because
  # they're pretty cluttered). EXTRA_LIBS is hard-coded for the Intel compiler
  # (currently rstm/cmake/modules/cxx-tm/Platform/Linux-Intel-CXX-tm.cmake).
  target_link_libraries(${${exec}} ${CMAKE_CXX-tm_EXTRA_LIBS})

  # Add the CXX-tm paths as -L paths
  foreach (path ${CMAKE_CXX-tm_LINK_DIRS${arch}})
    append_property(TARGET ${${exec}} LINK_FLAGS ${CMAKE_LIBRARY_PATH_FLAG}${path})
  endforeach ()
  
  # Finally, add an rpath to get to the correct CXX-tm directories.
  set_target_properties(${${exec}} PROPERTIES
    INSTALL_RPATH "${CMAKE_CXX-tm_LINK_DIRS${arch}}"
    BUILD_WITH_INSTALL_RPATH YES)
endmacro ()

## Add a multiarch executable that depends on the stm target library.
macro (add_stm_executable exec name arch)
  add_multiarch_executable(${exec} ${name} ${arch} ${ARGN})
  target_link_libraries(${${exec}} stm${arch})
endmacro ()

## Add a multiarch executable that uses Intel's itm library.
macro (add_itm_executable exec name arch)
  add_cxxtm_executable(${exec} ${name} ${arch} ${ARGN})
  target_link_libraries(${${exec}} ${CMAKE_CXX-tm_STM_LIBS})
endmacro ()

## Add a multiarch executable that uses the itm2stm shim (by definition this
## means that the executable is actually a cxxtm executable).
macro (add_itm2stm_executable exec name arch)
  add_cxxtm_executable(${exec} ${name} ${arch} ${ARGN})
  target_link_libraries(${${exec}} itm2stm${arch})
endmacro ()
