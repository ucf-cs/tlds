#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

## This file defines the package-wide user configuration options.  There are
## only three: the pointer size, and whether to build for the library API or
## for libitm.

## Include support for dependent options.  Such options can be completely
## hidden when their prereqs aren't satisfied.
include (CMakeDependentOption)

## This function allows us to check if the C++ compiler accepts
## certain flags, we use it to look for -m64 and -m32.
include (CheckCXXCompilerFlag)

## if we are in 32-bit mode, figure out if 64-bit builds are possible, and
## ask about whether to build 32-bit, 64-bit, or both.
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
  # testing for 64-bit support is a little weird: we need to add -m64 to the
  # CXXFLAGS and LDFLAGS
  set(CMAKE_REQUIRED_LIBRARIES -m64)
  check_cxx_compiler_flag(-m64 CXX_HAS_-m64)
  set(CMAKE_REQUIRED_LIBRARIES)

  # 32-bit building is on by default
  option(
    rstm_build_32-bit
    "Build 32-bit libraries and applications?" YES)

  # even if 64-bit is available, it is off by default
  cmake_dependent_option(
    rstm_build_64-bit
    "Build 64-bit libraries and applications?" NO
    "CXX_HAS_-m64" NO)

## Symmetric cases for 64-bit environment asking about building 32-bit code
elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(CMAKE_REQUIRED_LIBRARIES -m32)
  check_cxx_compiler_flag(-m32 CXX_HAS_-m32)
  set(CMAKE_REQUIRED_LIBRARIES)

  cmake_dependent_option(
    rstm_build_64-bit
    "Build 64-bit libraries and applications?" YES
    "CXX_HAS_-m32" YES)

  cmake_dependent_option(
    rstm_build_32-bit
    "Build 32-bit libraries and applicationss?" NO
    "CXX_HAS_-m32" NO)

## If pointers are not 32 or 64 bits, then print an error
else ()
  message(SEND_ERROR "Unexpected platform void* size: ${CMAKE_SIZEOF_VOID_P}")
endif ()

## Ask about building the ITM shim if we're on an x86 platform
cmake_dependent_option(
  rstm_enable_itm2stm
  "ON enables libitm2stm, and forces some libstm options for compatibility"
  OFF
  "CMAKE_SYSTEM_PROCESSOR MATCHES (x86_64|i.86)" OFF)

## Ask about building the stamp that we distribute in stamp-0.9.10
option(
  rstm_enable_stamp
  "ON to enable Stanford's STAMP benchmark suite." ON)

## Ask about building the mesh application (only available for 32-bits)
cmake_dependent_option(
  rstm_enable_mesh
  "ON to enable the mesh application." ON
  "rstm_build_32-bit" OFF)

## Ask about building the benchmark suite.
option(
  rstm_enable_bench
  "ON to enable the benchmark suite." ON)

## Ask about building with pure-itm.
cmake_dependent_option(
  rstm_enable_itm "Do you want to build apps with icc's native libitm.a?" OFF
  "CMAKE_CXX-tm_COMPILER_WORKS" OFF)
mark_as_advanced(rstm_enable_itm)
