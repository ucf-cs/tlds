#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

## This file sets up the base CXXFLAGS for all supported platforms
## (compiler/os/cpu).  We need to paramaterize based on the 'build type', and
## cmake has more defaults than 'O0 or O3', so that makes things a little more
## cumbersome.

## The first step is to create a 'string' for each common bunch of CXXFLAG
## options:

# Declare all of the initial flags that we care about
set(rstm_init_cxx_flags -Wall)
set(rstm_init_cxx_flags_Sun -mt -DHT_DEBUG +w -features=zla -template=no%extdef)
set(rstm_init_cxx_flags_Sun_x86 -xarch=sse2)
set(rstm_init_cxx_flags_Sun_sparc -xarch=native -xcode=pic32 -Qoption cg -Qiselect-movxtod=0,-Qiselect-movitof=0,-Qiselect-unfused_muladd=0,-Qiselect-sqrt1x=0,-Qiselect-fused_muladd=0)
set(rstm_init_cxx_flags_Sun_RELWITHDEB_INFO -O5 -g0)
set(rstm_init_cxx_flags_GNU_x86 -msse2 -mfpmath=sse -march=core2 -mtune=core2)
set(rstm_init_cxx_flags_GNU_sparc -mcpu=v9)
set(rstm_init_cxx_flags_GNU_RELWITHDEBINFO -O3 -g)
set(rstm_init_cxx_flags_Intel -wd981,1599 -vec_report0) # universally unhelpful
set(rstm_init_cxx_flags_Intel_DEBUG -g -debug all)
set(rstm_init_cxx_flags_Intel_x86_RELWITHDEBINFO -O3 -g -xSSE3) # icpc has no SSE2
set(rstm_init_cxx_flags_Intel_x86_RELEASE -O3 -DNDEBUG -xSSE3) # icpc has no SSE2


## Figure out a canonical name for the processor.
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|i.86)")
  set(arch x86)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
  set(arch sparc)
endif ()

# A macro that helps us append to strings in the cache. This loops through the
# passed list of parameters, and string-appends each one.
macro (append name)
  foreach (arg ${ARGN})
    set(${name} "${${name}} ${arg}")
  endforeach ()
endmacro ()

## Merge all of the configurable initial flags into their correct CMAKE cache
## positions.
foreach (type _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL "")
  foreach(lang CXX;CXX-tm)
    set(var "")
    append(var ${rstm_init_cxx_flags${type}})
    append(var ${rstm_init_cxx_flags_${CMAKE_${lang}_COMPILER_ID}${type}})
    append(var ${rstm_init_cxx_flags_${arch}${type}})
    append(var ${rstm_init_cxx_flags_${CMAKE_${lang}_COMPILER_ID}_${arch}${type}})
    if (var AND CMAKE_${lang}_COMPILER_ID)
      set(CMAKE_${lang}_FLAGS${type} ${var}
        CACHE STRING "Flags used by the compiler during ${type} builds.")
    endif ()
  endforeach ()
endforeach ()

## Force the default build type to be a release with debug info. This is
## traditional for the RSTM package.  It is also probably the only one that
## is correct right now.
set(
  CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
  "Choose the type of build: None;Debug;Release;RelWithDebInfo;MinSizeRel")
