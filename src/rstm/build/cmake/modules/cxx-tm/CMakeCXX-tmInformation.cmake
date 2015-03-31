#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

#
# This file sets the basic flags for the CXX-tm language in CMake.
#
# It is based on the C++ language flags file because CXX-tm is just a refinement
# of CXX.
# 
# It also loads the available platform file for the system-compiler if it
# exists. It also loads a system - compiler - processor (or target hardware)
# specific file, which is mainly useful for crosscompiling and embedded
# systems.

if (UNIX)
  set(CMAKE_CXX-tm_OUTPUT_EXTENSION .o)
else ()
  set(CMAKE_CXX-tm_OUTPUT_EXTENSION .obj)
endif ()

set(_INCLUDED_FILE 0)

# Load compiler-specific information.
if (CMAKE_CXX-tm_COMPILER_ID)
  include(
    Compiler/${CMAKE_CXX-tm_COMPILER_ID}-CXX-tm
    OPTIONAL)
endif ()

set(CMAKE_BASE_NAME)
get_filename_component(CMAKE_BASE_NAME ${CMAKE_CXX-tm_COMPILER} NAME_WE)

# load a hardware specific file, mostly useful for embedded compilers
if (CMAKE_SYSTEM_PROCESSOR)
  if (CMAKE_CXX-tm_COMPILER_ID)
    include(
      Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_CXX-tm_COMPILER_ID}-CXX-tm-${CMAKE_SYSTEM_PROCESSOR}
      OPTIONAL RESULT_VARIABLE _INCLUDED_FILE)
  endif ()
  if (NOT _INCLUDED_FILE)
    include(
      Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_BASE_NAME}-${CMAKE_SYSTEM_PROCESSOR}
      OPTIONAL)
  endif ()
endif ()

# load the system- and compiler specific files
if (CMAKE_CXX-tm_COMPILER_ID)
  include(
    Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_CXX-tm_COMPILER_ID}-CXX-tm
    OPTIONAL RESULT_VARIABLE _INCLUDED_FILE)
endif ()

if (NOT _INCLUDED_FILE)
  include(
    Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_BASE_NAME}
    OPTIONAL RESULT_VARIABLE _INCLUDED_FILE)
endif ()

# We specify the compiler information in the system file for some platforms,
# but this language may not have been enabled when the file was first included.
# Include it again to get the language info.  Remove this when all compiler
# info is removed from system files.
if (NOT _INCLUDED_FILE)
  include(Platform/${CMAKE_SYSTEM_NAME} OPTIONAL)
endif (NOT _INCLUDED_FILE)


# This should be included before the _INIT variables are used to initialize the
# cache.  Since the rule variables have if blocks on them, users can still
# define them here.  But, it should still be after the platform file so changes
# can be made to those values.
if (CMAKE_USER_MAKE_RULES_OVERRIDE)
  # Save the full path of the file so try_compile can use it.
  include (${CMAKE_USER_MAKE_RULES_OVERRIDE} RESULT_VARIABLE _override)
  set (CMAKE_USER_MAKE_RULES_OVERRIDE "${_override}")
endif ()

if (CMAKE_USER_MAKE_RULES_OVERRIDE_CXX-tm)
  # Save the full path of the file so try_compile can use it.
  include (${CMAKE_USER_MAKE_RULES_OVERRIDE_CXX-tm} RESULT_VARIABLE _override)
  set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX-tm "${_override}")
endif ()


# for most systems a module is the same as a shared library so unless the
# variable CMAKE_MODULE_EXISTS is set just copy the values from the LIBRARY
# variables
if (NOT CMAKE_MODULE_EXISTS)
  set(CMAKE_SHARED_MODULE_CXX-tm_FLAGS ${CMAKE_SHARED_LIBRARY_CXX-tm_FLAGS})
endif ()

# Create a set of shared library variable specific to C++ TM
if (NOT CMAKE_SHARED_LIBRARY_CREATE_CXX-tm_FLAGS)
  set(CMAKE_SHARED_LIBRARY_CREATE_CXX-tm_FLAGS ${CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS})
endif ()

if (NOT CMAKE_SHARED_LIBRARY_CXX-tm_FLAGS)
  set(CMAKE_SHARED_LIBRARY_CXX-tm_FLAGS ${CMAKE_SHARED_LIBRARY_CXX_FLAGS})
endif ()

if (NOT DEFINED CMAKE_SHARED_LIBRARY_LINK_CXX-tm_FLAGS)
  set(CMAKE_SHARED_LIBRARY_LINK_CXX-tm_FLAGS ${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS})
endif ()

if (NOT CMAKE_SHARED_LIBRARY_RUNTIME_CXX-tm_FLAG)
  set(CMAKE_SHARED_LIBRARY_RUNTIME_CXX-tm_FLAG ${CMAKE_SHARED_LIBRARY_RUNTIME_CXX_FLAG}) 
endif ()

if (NOT CMAKE_SHARED_LIBRARY_RUNTIME_CXX-tm_FLAG_SEP)
  set(CMAKE_SHARED_LIBRARY_RUNTIME_CXX-tm_FLAG_SEP ${CMAKE_SHARED_LIBRARY_RUNTIME_CXX_FLAG_SEP})
endif ()

if (NOT CMAKE_SHARED_LIBRARY_RPATH_LINK_CXX-tm_FLAG)
  set(CMAKE_SHARED_LIBRARY_RPATH_LINK_CXX-tm_FLAG ${CMAKE_SHARED_LIBRARY_RPATH_LINK_CXX_FLAG})
endif ()

if (NOT DEFINED CMAKE_EXE_EXPORTS_CXX-tm_FLAG)
  set(CMAKE_EXE_EXPORTS_CXX-tm_FLAG ${CMAKE_EXE_EXPORTS_CXX_FLAG})
endif ()

if (NOT DEFINED CMAKE_SHARED_LIBRARY_SONAME_CXX-tm_FLAG)
  set(CMAKE_SHARED_LIBRARY_SONAME_CXX-tm_FLAG ${CMAKE_SHARED_LIBRARY_SONAME_CXX_FLAG})
endif ()

if (NOT CMAKE_EXECUTABLE_RUNTIME_CXX-tm_FLAG)
  set(CMAKE_EXECUTABLE_RUNTIME_CXX-tm_FLAG ${CMAKE_SHARED_LIBRARY_RUNTIME_CXX-tm_FLAG})
endif ()

if (NOT CMAKE_EXECUTABLE_RUNTIME_CXX-tm_FLAG_SEP)
  set(CMAKE_EXECUTABLE_RUNTIME_CXX-tm_FLAG_SEP ${CMAKE_SHARED_LIBRARY_RUNTIME_CXX-tm_FLAG_SEP})
endif ()

if (NOT CMAKE_EXECUTABLE_RPATH_LINK_CXX-tm_FLAG)
  set(CMAKE_EXECUTABLE_RPATH_LINK_CXX-tm_FLAG ${CMAKE_SHARED_LIBRARY_RPATH_LINK_CXX-tm_FLAG})
endif ()

if (NOT DEFINED CMAKE_SHARED_LIBRARY_LINK_CXX-tm_WITH_RUNTIME_PATH)
  set(CMAKE_SHARED_LIBRARY_LINK_CXX-tm_WITH_RUNTIME_PATH
    ${CMAKE_SHARED_LIBRARY_LINK_CXX_WITH_RUNTIME_PATH})
endif ()

if (NOT CMAKE_INCLUDE_FLAG_CXX-tm)
  set(CMAKE_INCLUDE_FLAG_CXX-tm ${CMAKE_INCLUDE_FLAG_CXX})
endif ()

if (NOT CMAKE_INCLUDE_FLAG_SEP_CXX-tm)
  set(CMAKE_INCLUDE_FLAG_SEP_CXX-tm ${CMAKE_INCLUDE_FLAG_SEP_CXX})
endif ()

# repeat for modules
if (NOT CMAKE_SHARED_MODULE_CREATE_CXX-tm_FLAGS)
  set(CMAKE_SHARED_MODULE_CREATE_CXX-tm_FLAGS ${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS})
endif ()

if (NOT CMAKE_SHARED_MODULE_CXX-tm_FLAGS)
  set(CMAKE_SHARED_MODULE_CXX-tm_FLAGS ${CMAKE_SHARED_MODULE_CXX_FLAGS})
endif ()

# Initialize CXX-tm link type selection flags from C versions.
FOREACH(type SHARED_LIBRARY SHARED_MODULE EXE)
  if (NOT CMAKE_${type}_LINK_STATIC_CXX-tm_FLAGS)
    set(CMAKE_${type}_LINK_STATIC_CXX-tm_FLAGS
      ${CMAKE_${type}_LINK_STATIC_CXX_FLAGS})
  endif (NOT CMAKE_${type}_LINK_STATIC_CXX-tm_FLAGS)
  if (NOT CMAKE_${type}_LINK_DYNAMIC_CXX-tm_FLAGS)
    set(CMAKE_${type}_LINK_DYNAMIC_CXX-tm_FLAGS
      ${CMAKE_${type}_LINK_DYNAMIC_CXX_FLAGS})
  endif (NOT CMAKE_${type}_LINK_DYNAMIC_CXX-tm_FLAGS)
ENDFOREACH(type)

# add the flags to the cache based
# on the initial values computed in the platform/*.cmake files
# use _INIT variables so that this only happens the first time
# and you can set these flags in the cmake cache
set(CMAKE_CXX-tm_FLAGS_INIT "$ENV{CXXFLAGS} ${CMAKE_CXX-tm_FLAGS_INIT}")
# avoid just having a space as the initial value for the cache 
if (CMAKE_CXX-tm_FLAGS_INIT STREQUAL " ")
  set(CMAKE_CXX-tm_FLAGS_INIT)
endif (CMAKE_CXX-tm_FLAGS_INIT STREQUAL " ")
SET(CMAKE_CXX-tm_FLAGS "${CMAKE_CXX-tm_FLAGS_INIT}" CACHE STRING
  "Flags used by the compiler during all build types.")

if (NOT CMAKE_NOT_USING_CONFIG_FLAGS)
  SET(CMAKE_CXX-tm_FLAGS_DEBUG
    "${CMAKE_CXX-tm_FLAGS_DEBUG_INIT}" CACHE STRING
    "Flags used by the compiler during debug builds.")
  SET(CMAKE_CXX-tm_FLAGS_MINSIZEREL
    "${CMAKE_CXX-tm_FLAGS_MINSIZEREL_INIT}" CACHE STRING
    "Flags used by the compiler during release minsize builds.")
  SET(CMAKE_CXX-tm_FLAGS_RELEASE
    "${CMAKE_CXX-tm_FLAGS_RELEASE_INIT}" CACHE STRING
    "Flags used by the compiler during release builds (/MD /Ob1 /Oi /Ot /Oy /Gs will produce slightly less optimized but smaller files).")
  SET(CMAKE_CXX-tm_FLAGS_RELWITHDEBINFO
    "${CMAKE_CXX-tm_FLAGS_RELWITHDEBINFO_INIT}" CACHE STRING
    "Flags used by the compiler during Release with Debug Info builds.")

endif ()

if (CMAKE_CXX-tm_STANDARD_LIBRARIES_INIT)
  set(CMAKE_CXX-tm_STANDARD_LIBRARIES "${CMAKE_CXX-tm_STANDARD_LIBRARIES_INIT}"
    CACHE STRING "Libraries linked by defalut with all C++ TM applications.")
  MARK_AS_ADVANCED(CMAKE_CXX-tm_STANDARD_LIBRARIES)
endif ()

include (CMakeCommonLanguageInclude)

# NB: All we use the CXX-tm language for is to generate objet files that can then
#     be linked using C++. We'll need more work here. If we need to link with
#     the CXX-tm compiler.
#
# now define the following rules:
# CMAKE_CXX-tm_CREATE_SHARED_LIBRARY
# CMAKE_CXX-tm_CREATE_SHARED_MODULE
# CMAKE_CXX-tm_COMPILE_OBJECT
# CMAKE_CXX-tm_LINK_EXECUTABLE

# variables supplied by the generator at use time
# <TARGET>
# <TARGET_BASE> the target without the suffix
# <OBJECTS>
# <OBJECT>
# <LINK_LIBRARIES>
# <FLAGS>
# <LINK_FLAGS>

# CXX-tm compiler information
# <CMAKE_CXX-tm_COMPILER>  
# <CMAKE_SHARED_LIBRARY_CREATE_CXX-tm_FLAGS>
# <CMAKE_CXX-tm_SHARED_MODULE_CREATE_FLAGS>
# <CMAKE_CXX-tm_LINK_FLAGS>

# Static library tools
# <CMAKE_AR> 
# <CMAKE_RANLIB>

# create a shared C++ library
if (NOT CMAKE_CXX-tm_CREATE_SHARED_LIBRARY)
  set(CMAKE_CXX-tm_CREATE_SHARED_LIBRARY
    "<CMAKE_CXX-tm_COMPILER> <CMAKE_SHARED_LIBRARY_CXX-tm_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX-tm_FLAGS> <CMAKE_SHARED_LIBRARY_SONAME_CXX-tm_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")
endif (NOT CMAKE_CXX-tm_CREATE_SHARED_LIBRARY)

# create a c++ shared module copy the shared library rule by default
if (NOT CMAKE_CXX-tm_CREATE_SHARED_MODULE)
  set(CMAKE_CXX-tm_CREATE_SHARED_MODULE ${CMAKE_CXX-tm_CREATE_SHARED_LIBRARY})
endif (NOT CMAKE_CXX-tm_CREATE_SHARED_MODULE)

# Create a static archive incrementally for large object file counts.
# If CMAKE_CXX-tm_CREATE_STATIC_LIBRARY is set it will override these.
if (NOT DEFINED CMAKE_CXX-tm_ARCHIVE_CREATE)
  set(CMAKE_CXX-tm_ARCHIVE_CREATE "<CMAKE_AR> cr <TARGET> <LINK_FLAGS> <OBJECTS>")
endif ()

if (NOT DEFINED CMAKE_CXX-tm_ARCHIVE_APPEND)
  set(CMAKE_CXX-tm_ARCHIVE_APPEND "<CMAKE_AR> r  <TARGET> <LINK_FLAGS> <OBJECTS>")
endif ()

if (NOT DEFINED CMAKE_CXX-tm_ARCHIVE_FINISH)
  set(CMAKE_CXX-tm_ARCHIVE_FINISH "<CMAKE_RANLIB> <TARGET>")
endif ()

# compile a C++ file into an object file
if (NOT CMAKE_CXX-tm_COMPILE_OBJECT)
  set(CMAKE_CXX-tm_COMPILE_OBJECT
    "<CMAKE_CXX-tm_COMPILER>  <DEFINES> <FLAGS> -o <OBJECT> -c -x c++ <SOURCE>")
endif ()

if (NOT CMAKE_CXX-tm_LINK_EXECUTABLE)
  set(CMAKE_CXX-tm_LINK_EXECUTABLE
    "<CMAKE_CXX-tm_COMPILER>  <FLAGS> <CMAKE_CXX-tm_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
endif ()

mark_as_advanced (
  CMAKE_BUILD_TOOL
  CMAKE_VERBOSE_MAKEFILE 
  CMAKE_CXX-tm_FLAGS
  CMAKE_CXX-tm_FLAGS_RELEASE
  CMAKE_CXX-tm_FLAGS_RELWITHDEBINFO
  CMAKE_CXX-tm_FLAGS_MINSIZEREL
  CMAKE_CXX-tm_FLAGS_DEBUG)

set(CMAKE_CXX-tm_INFORMATION_LOADED 1)
