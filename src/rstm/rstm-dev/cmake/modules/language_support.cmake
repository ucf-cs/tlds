#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

#
# http://public.kitware.com/Bug/view.php?id=9220
#
# This file provides a workaround for cmake bug 9220, which makes it
# impossible to directly use the OPTIONAL tag for registering a language. We
# use it in rstm for our CXX-tm language support.

# Temporary additional general language support is contained within this
# file.  

# This additional function definition is needed to provide a workaround for
# CMake bug 9220.

function(workaround_9220 language language_works)
  #message("DEBUG: language = ${language}")
  set(text
    "project(test NONE)
cmake_minimum_required(VERSION 2.8.0)
set (CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules/cxx-tm)
enable_language(${language} OPTIONAL)
if (NOT CMAKE_CXX-tm_COMPILER_WORKS)
  message(FATAL_ERROR \"Configuring CCX-tm failed (expected)\")
endif ()
"
    )
  file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/language_tests/${language})
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/language_tests/${language})
  file(WRITE ${CMAKE_BINARY_DIR}/language_tests/${language}/CMakeLists.txt
    ${text})
  execute_process(
    COMMAND ${CMAKE_COMMAND} .
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/language_tests/${language}
    RESULT_VARIABLE return_code
    OUTPUT_QUIET
    ERROR_QUIET
    )
  if(return_code EQUAL 0)
    set(${language_works} ON PARENT_SCOPE)
  else(return_code EQUAL 0)
    set(${language_works} OFF PARENT_SCOPE)
  endif(return_code EQUAL 0)
endfunction(workaround_9220)

# Temporary tests of the above function.
#workaround_9220(CXX CXX_language_works)
#message("CXX_language_works = ${CXX_language_works}")
#workaround_9220(CXXp CXXp_language_works)
#message("CXXp_language_works = ${CXXp_language_works}")


