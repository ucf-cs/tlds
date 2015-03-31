#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

include (CustomAddExecutable)
include (AddTargetDefinitions)
include (AppendProperty)

# This macro adds a stamp executable that uses stamp's STM interface and links
# to libstm.a through the STM_API_STAMP interface.
macro (add_stamp_executable exec name arch)
  # Force the use of the C++ compiler
  set_source_files_properties(${ARGN} PROPERTIES LANGUAGE CXX)

  # Add the executable
  add_stm_executable(${exec} ${name}STM ${arch} ${ARGN})

  # Link with the pthreads library if needed.
  target_link_libraries(${${exec}} ${CMAKE_THREAD_LIBS_INIT})

  # Set some baseline defines.
  add_target_definitions(${${exec}} STM STM_API_STAMP HAVE_CONFIG_H)
endmacro ()

# Utility to copy sources that are sometimes C++ and sometimes C++-tm sources to
# build-directory sources with .cxxtm appended.
macro (copy_to_cxxtm sources_out)
  # We need to make a version of the source file that we can compile with a
  # C++-tm compiler. We can't just set the language property of each file
  # because it might be used in a regular STM build, and sources can't be
  # treated as different languages for different builds.
  foreach (source ${ARGN})
    if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${source}.cxxtm)
      configure_file(${source} ${source}.cxxtm COPYONLY)
      append_list_property(
        SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${source}.cxxtm
        OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source})
    endif ()
    list(APPEND ${sources_out} ${source}.cxxtm)
  endforeach ()
endmacro ()

# This macro adds a stamp executable that uses the C++ STM API and links to
# Intel's libitm.a.
macro (add_stamp_itm_executable exec name arch)
  set(cxxtm_souces)
  copy_to_cxxtm(cxxtm_sources ${ARGN})
  
  # Add the itm executable.
  add_itm_executable(${exec} ${name}ITM ${arch} ${cxxtm_sources})
  
  # When we copy sources like that we need to make sure that they can find the
  # right directory.
  append_property(TARGET ${${exec}} COMPILE_FLAGS -I${CMAKE_CURRENT_SOURCE_DIR})

  # link with whatever threading library we need.
  target_link_libraries(${${exec}} ${CMAKE_THREAD_LIBS_INIT})

  # and set the baseline definitions
  add_target_definitions(${${exec}} CXXTM ITM HAVE_CONFIG_H)
endmacro ()

# This does the same thing as add_stamp_itm_executable except that it uses the
# add_itm2stm_executable underlying call. If we could template based on this
# call (or generate a function binding as add_stamp_${type}_executable we could
# remove this).
macro (add_stamp_itm2stm_executable exec name arch)
  set(cxxtm_souces)
  copy_to_cxxtm(cxxtm_sources ${ARGN})
  
  # Add the itm executable.
  add_itm2stm_executable(${exec} ${name}SHIM ${arch} ${cxxtm_sources})
  
  # When we copy sources like that we need to make sure that they can find the
  # right directory.
  append_property(TARGET ${${exec}} COMPILE_FLAGS -I${CMAKE_CURRENT_SOURCE_DIR})

  # link with whatever threading library we need.
  target_link_libraries(${${exec}} ${CMAKE_THREAD_LIBS_INIT})

  # and set the baseline definitions
  add_target_definitions(${${exec}} CXXTM ITM HAVE_CONFIG_H)
endmacro ()

configure_file(STAMP_config.h.cmake STAMP_config.h)
include_directories(lib)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(bayes)
add_subdirectory(genome)
add_subdirectory(intruder)
add_subdirectory(kmeans)
add_subdirectory(labyrinth)
add_subdirectory(ssca2)
add_subdirectory(vacation)
add_subdirectory(yada)
