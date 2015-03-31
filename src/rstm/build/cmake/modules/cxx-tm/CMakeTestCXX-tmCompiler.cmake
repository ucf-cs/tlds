
#=============================================================================
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
#=============================================================================
include (CMakeTestCompilerCommon)

# This file is used by EnableLanguage in cmGlobalGenerator to
# determine that that selected C++ TM compiler can actually compile
# and link the most basic of programs.   If not, a fatal error
# is set and cmake stops processing commands and will not generate
# any makefiles or projects.
if (NOT CMAKE_CXX-tm_COMPILER_WORKS)
  PrintTestCompilerStatus("CXX-tm" "")
  file (WRITE ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/testCXX-tmCompiler.cxxtm 
    "#ifndef __cplusplus\n"
    "# error \"The CMAKE_CXX-tm_COMPILER is set to a C compiler\"\n"
    "#endif\n"
    "int main(){return 0;}\n")
  try_compile (CMAKE_CXX-tm_COMPILER_WORKS ${CMAKE_BINARY_DIR} 
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/testCXX-tmCompiler.cxxtm
    OUTPUT_VARIABLE OUTPUT)
  set (CXX-tm_TEST_WAS_RUN 1)
endif (NOT CMAKE_CXX-tm_COMPILER_WORKS)

if (NOT CMAKE_CXX-tm_COMPILER_WORKS)
  PrintTestCompilerStatus("CXX-tm" " -- broken")
  # if the compiler is broken make sure to remove the platform file
  # since Windows-cl configures both c/cxx files both need to be removed
  # when c or c++ fails
  file (REMOVE ${CMAKE_PLATFORM_ROOT_BIN}/CMakeCPlatform.cmake )
  file (REMOVE ${CMAKE_PLATFORM_ROOT_BIN}/CMakeCXX-tmPlatform.cmake )
  file (APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
    "Determining if the CXX-tm compiler works failed with "
    "the following output:\n${OUTPUT}\n\n")
  message (FATAL_ERROR "The C++ TM compiler \"${CMAKE_CXX-tm_COMPILER}\" "
    "is not able to compile a simple test program.\nIt fails "
    "with the following output:\n ${OUTPUT}\n\n"
    "CMake will not be able to correctly generate this project.")
else (NOT CMAKE_CXX-tm_COMPILER_WORKS)
  if (CXX-tm_TEST_WAS_RUN)
    PrintTestCompilerStatus("CXX-tm" " -- works")
    file (APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
      "Determining if the CXX-tm compiler works passed with "
      "the following output:\n${OUTPUT}\n\n")
  endif (CXX-tm_TEST_WAS_RUN)
  set (CMAKE_CXX-tm_COMPILER_WORKS 1 CACHE INTERNAL "")

  if (CMAKE_CXX-tm_COMPILER_FORCED)
    # The compiler configuration was forced by the user.
    # Assume the user has configured all compiler information.
  else ()
    configure_file (
      ${CMAKE_CURRENT_LIST_DIR}/CMakeCXX-tmCompilerABI.cxxtm.in
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeCXX-tmCompilerABI.cxxtm
      IMMEDIATE)
    # Try to identify the ABI and configure it into CMakeCXX-tmCompiler.cmake
    include (${CMAKE_ROOT}/Modules/CMakeDetermineCompilerABI.cmake)
    cmake_determine_compiler_abi (CXX-tm
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeCXX-tmCompilerABI.cxxtm)

    # At this point, we have learned something about the compiler, in particular
    # we have deduced the CMAKE_CXX-tm_IMPLICIT_LINK_DIRECTORIES. These are
    # automatically added to linking multi-language targets, if we remember
    # them. The problem is that they are only valid for the native architecture
    # (i.e., 32 or 64-bit compilation).
    #
    # We need to translate these into 32 and 64-bit variables:
    #  CMAKE_CXX-tm_LINK_DIRS32
    #  CMAKE_CXX-tm_LINK_DIRS64
    #
    # If the implicit directories are for 32 bits, and we need a 64-bit version,
    # we can use the DIRS_REGEX32/64 to do the replacement. In the same way, we
    # can get a 32-bit version if we have the 64-bit version. The nice part
    # about this is that the 64-bit version won't get any replacement results if
    # we apply the 32-bit REGEX to it, so the output is just the input.
    foreach (dir ${CMAKE_CXX-tm_IMPLICIT_LINK_DIRECTORIES})
      list(FIND CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES ${dir} dir_FOUND)
      if (dir_FOUND EQUAL -1)
        string(REPLACE
          ${CMAKE_CXX-tm_LINK_DIRS32_REGEX}
          ${CMAKE_CXX-tm_LINK_DIRS64_REGEX}
          dir64 ${dir})
        list(APPEND CMAKE_CXX-tm_LINK_DIRS64 ${dir64})
        
        string(REPLACE
          ${CMAKE_CXX-tm_LINK_DIRS64_REGEX}
          ${CMAKE_CXX-tm_LINK_DIRS32_REGEX}
          dir32 ${dir})
        list(APPEND CMAKE_CXX-tm_LINK_DIRS32 ${dir32})
      endif ()
    endforeach ()
    
    configure_file (
      ${CMAKE_CURRENT_LIST_DIR}/CMakeCXX-tmCompiler.cmake.in
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeCXX-tmCompiler.cmake
      IMMEDIATE)
    include (${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeCXX-tmCompiler.cmake)
  endif ()
endif (NOT CMAKE_CXX-tm_COMPILER_WORKS)
