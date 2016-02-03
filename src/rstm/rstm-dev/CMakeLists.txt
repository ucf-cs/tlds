#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

# This appears to be the recommended method for overriding the CMake default
# compiler and linking rules and variables.
set(CMAKE_USER_MAKE_RULES_OVERRIDE
    ${CMAKE_SOURCE_DIR}/cmake/UserOverrides.cmake)

# Our top-level configuration. We include the "C" language because otherwise
# we have trouble with some of the standard configuration packages.
project(rstm C CXX)

# This may work on other versions of cmake, but we've never done any testing
# so it's safer for us to be conservative. If you have an earlier version
# that you can confirm works please contact us and we can update this
# requirement.
cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

set(PACKAGE rstm)

# Add local cmake stuff.
set(
  CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/cmake/modules
  ${CMAKE_SOURCE_DIR}/cmake/modules/cxx-tm)

# Enable our custom CXXTM language, which essentially just drags in a second
# C++ compiler that hopefully has support for the C++ TM draft. This compiler
# is used for .cxxtm files that are part of an executable target. We _never_
# use it for linking, or for objets that will be part of a library.
include (language_support)
workaround_9220(CXX-tm CXX-tm_WORKS)
if (CXX-tm_WORKS)
  enable_language(CXX-tm)
endif ()

# We use pthreads in a number of places, so find them here.
set(CMAKE_THREAD_PREFER_PTHREAD 1)
find_package(Threads)
find_package(OpenMP)

# The threads package is sort of wonky in that it doesn't set any compile
# flags, just a link flag.
#
# On some platforms, the link flag is something like -thread, and on others the
# link flag is actually just the pthreads library (-lpthread). We can just
# universally add the link flag to the target link flags.
#
# This is more complicated for setting CXXFLAGS. On the platforms where the
# LIBS_INIT is actually a flag, we want to add that to CXXFLAGS. On the
# platforms where the LIBS_LIBS is actually a library, we get warnings if we
# try and add the LIBS_INIT to the CXXFLAGS. We deal with this by definining
# another variable for the include-side
#
string(REGEX MATCH "-l" CMAKE_THREAD_LIBS_IS_LIB ${CMAKE_THREAD_LIBS_INIT})
if (NOT CMAKE_THREAD_LIBS_IS_LIB)
  set(CMAKE_THREAD_INCLUDE ${CMAKE_THREAD_LIBS_INIT})
endif ()

# Include all fo the relevant UserConfig modules.
include (UserConfig.cmake)
include (libstm/UserConfig.cmake)
if (rstm_enable_mesh)
  include (mesh/UserConfig.cmake)
endif ()
if (rstm_enable_bench)
  include (bench/UserConfig.cmake)
endif ()
if (rstm_enable_stamp)
  include (stamp-0.9.10/UserConfig.cmake)
endif ()

# Build a list of the architectures (-m32/63) that we have configured. This is
# used all over the place for multilib-like builds.
set(rstm_archs)
if (rstm_build_32-bit)
  list(APPEND rstm_archs 32)
endif ()
if (rstm_build_64-bit)
  list(APPEND rstm_archs 64)
endif ()

include_directories (
  ${CMAKE_SOURCE_DIR}           # alt-license is a top-level directory
  ${CMAKE_SOURCE_DIR}/include   # standard project headers
  ${CMAKE_BINARY_DIR}/include   # configured headers
  )

add_subdirectory(include)     # configures stm/config.h.cmake
add_subdirectory (libstm)     # the main library
if (rstm_enable_itm2stm)
  add_subdirectory (libitm2stm) # the shim library
endif ()

if (CMAKE_USE_PTHREADS_INIT)
  if (rstm_enable_bench)
    add_subdirectory (bench) # build bench
  endif ()
  if (rstm_enable_stamp)
    add_subdirectory(stamp-0.9.10) # build stamp
  endif ()
  if (rstm_enable_mesh)
    add_subdirectory(mesh) # build mesh
  endif ()
else ()
  message (STATUS "Pthreads not found, will not build pthread apps.")
endif ()
