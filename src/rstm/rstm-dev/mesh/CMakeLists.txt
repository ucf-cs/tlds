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

set(
  sources
  dwyer.cpp
  edge.cpp
  edge_set.cpp
  mesh.cpp
  my_thread.cpp
  point.cpp
  queues.cpp
  side.cpp
  worker.cpp)

set(cxxtm_sources)

# If we're going to build with a C++ TM compiler then we need sources that are
# recognized as C++ TM sources (we can't just set the LANGUAGE property on the
# .cpps because we might want them to be plain C++ for the FGL, CGL, and STM
# builds). 
if (rstm_enable_itm OR rstm_enable_itm2stm)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR})
  foreach (source ${sources})
    configure_file(${source} ${source}.cxxtm COPYONLY)
    append_list_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${source}.cxxtm
      OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source})
    list(APPEND cxxtm_sources ${source}.cxxtm)
  endforeach ()
endif ()

# We need libstdc++ headers from the gcc 4.3 branch.
if (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  append_cxx_flags(${mesh_stdcxx-v3_include})
endif ()

if (CMAKE_CXX-tm_COMPILER_ID MATCHES "Intel")
  append_cxxtm_flags(-I${mesh_libstdcxx-v3_include})
endif ()

set (rt)
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(rt rt)
endif ()

# We onl support 32-bit builds with mesh
foreach (arch 32) #${rstm_archs})
  if (mesh_enable_cgl)
    add_multiarch_executable(exec meshCGL ${arch} ${sources})
    add_target_definitions(${exec} CGL)
    target_link_libraries(${exec} ${CMAKE_THREAD_LIBS_INIT} ${rt})
  endif ()
  
  if (mesh_enable_fgl)
    add_multiarch_executable(exec meshFGL ${arch} ${sources})
    add_target_definitions(${exec} FGL)
    target_link_libraries(${exec} ${CMAKE_THREAD_LIBS_INIT} ${rt})
  endif ()

  
  # if (rstm_enable_libstm)
  #   add_stm_executable(exec meshSTM ${arch} ${sources})
  #   add_target_definitions(${exec} STM)
  #   target_link_libraries(${exec} ${CMAKE_THREAD_LIBS_INIT})
  # endif ()
  
  if (rstm_enable_itm)
    add_itm_executable(exec meshITM ${arch} ${cxxtm_sources})
    add_target_definitions(${exec} ITM)
    target_link_libraries(${exec} ${CMAKE_THREAD_LIBS_INIT} ${rt})
  endif ()

  if (rstm_enable_itm2stm)
    add_itm2stm_executable(exec meshSHIM ${arch} ${cxxtm_sources})
    add_target_definitions(${exec} ITM)
    target_link_libraries(${exec} ${CMAKE_THREAD_LIBS_INIT})

    # So that #include "itm.h" works
    append_property(TARGET ${exec} COMPILE_FLAGS
      -I${CMAKE_SOURCE_DIR}/include/itm)
  endif ()
endforeach ()
