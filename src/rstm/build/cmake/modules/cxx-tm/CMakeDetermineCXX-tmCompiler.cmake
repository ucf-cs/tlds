#==============================================================================
# CMake - Cross Platform Makefile Generator
# Copyright 2000-2009 Kitware, Inc., Insight Software Consortium
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the names of Kitware, Inc., the Insight Software Consortium,
#   nor the names of their contributors may be used to endorse or promote
#   products derived from this software without specific prior written
#   permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Determine what the C++ TM compiler is.
#
# This is basically just taken from CMake's existing CXX language support, with
# the CXX stuff replaced with CXX-tm.
#
# This file only runs once, once it's run we configure a cmake.in file that
# caches what we've learned.

if (NOT CMAKE_CXX-tm_COMPILER)
  # if we don't have a cached CXX-tm compiler...
  
  # As with all languages, we prefer the environment variable---CXXTM in this
  # case.
  if ($ENV{CXXTM} MATCHES ".+")
    # The "PROGRAM" version of this function outputs any flags embedded in the
    # environmental variable into CMAKE_CXX-tm_FLAGS_ENV_INIT
    get_filename_component(
      CMAKE_CXX-tm_COMPILER_INIT $ENV{CXXTM}
      PROGRAM PROGRAM_ARGS CMAKE_CXX-tm_FLAGS_ENV_INIT)
    
    # If we have flags (say -Qtm_enabled) then we remember them in the a cached
    # variable. As a side note, this means that we extract a compiler too.
    if (CMAKE_CXX-tm_FLAGS_ENV_INIT)
      set(
        CMAKE_CXX-tm_COMPILER_ARG1 "${CMAKE_CXX-tm_FLAGS_ENV_INIT}" CACHE
        STRING "First argument to CXX-tm compiler")      
    endif ()
    
    # If we didn't find a compiler in the environment string (i.e., the string
    # is set, but the get_filename_component failed), we have and error.
    if (NOT EXISTS ${CMAKE_CXX-tm_COMPILER_INIT})
      message (SEND_ERROR
        "Could not find compiler in environment variable CXXTM: $ENV{CXXTM}.")
    endif ()
  endif ()

  # As is standard, we next try prefer the compiler specified by the generator.
  if (CMAKE_GENERATOR_CXX-tm) 
    if (NOT CMAKE_CXX-tm_COMPILER_INIT)
      set(CMAKE_CXX-tm_COMPILER_INIT ${CMAKE_GENERATOR_CXX-tm})
    endif ()
  endif ()
  
  # If we didn't find a compiler in the environment, and one isn't specified by
  # the generator, then we'll need to try and find one ourself. We will prefer
  # the existing CXX compiler. If that fails, we'll need to do a bunch of work
  # to check compilers and see if they support the CXX TM standard.
  #
  # Right now, we just fail over to icpc, which is the only CXX-tm compiler
  # available.
  if (CMAKE_CXX-tm_COMPILER_INIT)
    set(
      CMAKE_CXX-tm_COMPILER ${CMAKE_CXX-tm_COMPILER_INIT} CACHE PATH
      "C++ TM Compiler")
  else ()
    find_program(CMAKE_CXX-tm_COMPILER NAMES icpc DOC "C++ TM Compiler")
  endif ()

else ()
  # We only get here if CMAKE_CXX-tm_COMPILER was specified using -D or a
  # pre-made CMakeCache.txt (e.g. via ctest) or set in CMAKE_TOOLCHAIN_FILE.
  #
  # If CMAKE_CXX-tm_COMPILER is a list of length 2, use the first item as
  # CMAKE_CXX-tm_COMPILER and the 2nd one as CMAKE_CXX-tm_COMPILER_ARG1.
  list(LENGTH CMAKE_CXX-tm_COMPILER _CMAKE_CXX-tm_COMPILER_LIST_LENGTH)
  if ("${_CMAKE_CXX-tm_COMPILER_LIST_LENGTH}" EQUAL 2)
    list(GET CMAKE_CXX-tm_COMPILER 1 CMAKE_CXX-tm_COMPILER_ARG1)
    list(GET CMAKE_CXX-tm_COMPILER 0 CMAKE_CXX-tm_COMPILER)
  endif ()

  # If a compiler was specified by the user but without path, now try to find it
  # with the full path. If it is found, force it into the cache, if not, don't
  # overwrite the setting (which was given by the user) with "NOTFOUND".
  get_filename_component(
    _CMAKE_USER_CXX-tm_COMPILER_PATH "${CMAKE_CXX-tm_COMPILER}" PATH)
  
  if (NOT _CMAKE_USER_CXX-tm_COMPILER_PATH)
    find_program(CMAKE_CXX-tm_COMPILER_WITH_PATH NAMES ${CMAKE_CXX-tm_COMPILER})
    mark_as_advanced(CMAKE_CXX-tm_COMPILER_WITH_PATH)
    if (CMAKE_CXX-tm_COMPILER_WITH_PATH)
      set(
        CMAKE_CXX-tm_COMPILER ${CMAKE_CXX-tm_COMPILER_WITH_PATH} CACHE STRING
        "C++ TM Compiler" FORCE)
    endif ()
  endif ()
endif ()
mark_as_advanced (CMAKE_CXX-tm_COMPILER)

# If a toolchain has been specified (we're unlikely to have this happen yet, but
# it's in there for the future).
if (NOT _CMAKE_TOOLCHAIN_LOCATION)
  get_filename_component(
    _CMAKE_TOOLCHAIN_LOCATION "${CMAKE_CXX-tm_COMPILER}" PATH) 
endif (NOT _CMAKE_TOOLCHAIN_LOCATION)

# Now tht we have a compiler binary available, we want to figure out what it's
# id is (the name could be any crazy thing).
if (NOT CMAKE_CXX-tm_COMPILER_ID_RUN)
  set(CMAKE_CXX-tm_COMPILER_ID_RUN 1)

  # Each entry in this list is a set of extra flags to try adding to the
  # compile line to see if it helps produce a valid identification file.
  set(CMAKE_CXX-tm_COMPILER_ID_TEST_FLAGS
    "-c"
    )

  # Try to identify the compiler. This uses some existing files and tries to
  # compile them with the test flags above. The CXX-tm compiler is just a C++
  # compiler with some extra support, so we use the basic CXX infrastructure to
  # get an id.
  set(CMAKE_CXX-tm_COMPILER_ID)
  file(
    READ ${CMAKE_ROOT}/Modules/CMakePlatformId.h.in
    CMAKE_CXX-tm_COMPILER_ID_PLATFORM_CONTENT)
  include(${CMAKE_ROOT}/Modules/CMakeDetermineCompilerId.cmake)
  cmake_determine_compiler_id(CXX-tm CXXFLAGS CMakeCXXCompilerId.cpp)
endif ()

# configure all variables set in this file
configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/CMakeCXX-tmCompiler.cmake.in
  ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeCXX-tmCompiler.cmake
  IMMEDIATE)

SET(CMAKE_CXX-tm_COMPILER_ENV_VAR "CXXTM")

