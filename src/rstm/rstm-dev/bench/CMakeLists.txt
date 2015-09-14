#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

# This macro makes it trivial to append properties to source files and targets. 
include (AppendProperty)
include (AppendFlags)
include (CustomAddExecutable)
include (AddTargetDefinitions)

# List the base benchmarks that we are going to build. We assume that each
# benchmark consists of a file named <benchmark>.cpp, along with the
# bmharness.cpp file. CMake is going to recompile bmharness.cpp for every
# executable, which is annoying, but there doesn't seem to be an easy way to fix
# this (it's a CMake FAQ).
set(
  benchmarks
  CounterBench
  TreeBench
  ListBench
  DListBench
  WWPathologyBench
  HashBench
  ForestBench
  TreeOverwriteBench
  TypeTest
  DisjointBench
  MCASBench
  ReadWriteNBench
  ReadNWrite1Bench)

append_cxx_flags(${CMAKE_THREAD_INCLUDE})

# Build the STM executables.
if (bench_enable_multi_source)
  foreach (bench ${benchmarks})
    foreach (arch ${rstm_archs})
      add_stm_executable(exec "${bench}STM" ${arch} bmharness.cpp ${bench}.cpp)
      target_link_libraries(${exec} ${CMAKE_THREAD_LIBS_INIT})
    endforeach ()
  endforeach ()
endif ()

# Build the single-source executables.
if (bench_enable_single_source)
  foreach (bench ${benchmarks})
    foreach (arch ${rstm_archs})
      add_stm_executable(exec "${bench}SSB" ${arch} ${bench}.cpp)
      target_link_libraries(${exec} ${CMAKE_THREAD_LIBS_INIT})
      add_target_definitions(${exec} SINGLE_SOURCE_BUILD)
    endforeach ()
  endforeach ()
endif ()
# Build the CXX-tm executables, if the user has a configuration that is
# appropriate.
if (CMAKE_CXX-tm_COMPILER)
  if (NOT rstm_enable_itm2stm AND NOT rstm_enable_itm)
    break()
  endif ()
  include (AddTargetDefinitions)
  
  # We're going to be copying source files into the build directory, and trying
  # to compile them. We need to make sure that their relative addressing for
  # includes still works.
  include_directories(${CMAKE_CURRENT_SOURCE_DIR})

  # We need to use the _ITM_initialize/finalize macros, which are defined in
  # itm.h
  append_cxx_flags(-include itm/itm.h)
  
  foreach (bench ${benchmarks})
    # the only obvious way to compile a file twice with different compilers is
    # to copy the file into the build tree and use a different
    # extension. Because we're copying, we could use CMake's configuration
    # subsystem to configure the MACRO interface, but we don't and instead stick
    # with the traditional API.
    configure_file(${bench}.cpp ${bench}.cxxtm COPYONLY)
    append_list_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${bench}.cxxtm
      OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${bench}.cpp)

    
    foreach (arch ${rstm_archs})
      # Create pure itm versions of the apps if the user wants them. This is
      # good as a performance and correctness comparison with the shim.
      if (rstm_enable_itm)
        add_itm_executable(exec "${bench}ITM" ${arch} bmharness.cpp
          ${bench}.cxxtm)
    
        # Add target-specific definitions, which allows us to re-use the
        # bmharness file.
        add_target_definitions(${exec} ITM STM_API_CXXTM)

        # Link to the threading library.
        target_link_libraries(${exec} ${CMAKE_THREAD_LIBS_INIT})

        # There isn't a built-in depenence on rt in this build, so link to it
        # now, if necessary.
        if (CMAKE_SYSTEM_NAME MATCHES "Linux")
          target_link_libraries(${exec} rt)
        endif ()
      endif ()

      # Create the shim version of the application.
      if (rstm_enable_itm2stm)
        add_itm2stm_executable(exec "${bench}SHIM" ${arch} bmharness.cpp
          ${bench}.cxxtm)

        # Add target-specific definitions, which allows us to re-use the
        # bmharness file.
        add_target_definitions(${exec} ITM2STM STM_API_CXXTM)

        # Link with the threading libraryy
        target_link_libraries(${exec} ${CMAKE_THREAD_LIBS_INIT})
      endif ()

      # # For debugging purposes I create an STM implementation, but using the
      # # CXX-tm compiler to do it. This lets me check to see if the CXX-tm
      # # compiler is having any trouble with our benchmark code.
      # add_cxxtm_executable(exec "${bench}CXXTM" ${arch} bmharness.cpp
      #   ${bench}.cxxtm)

      # target_link_libraries(${exec} stm${arch} ${CMAKE_THREAD_LIBS_INIT})
      # if (CMAKE_SYSTEM_NAME MATCHES "Linux")
      #   target_link_libraries(${exec} -lrt)
      # endif ()
    endforeach ()
  endforeach ()
endif ()
