#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

set(CMAKE_CXX-tm_STM_LIBS -litm -ldl)
set(CMAKE_CXX-tm_LINK_DIRS32_REGEX "/lib/ia32")
set(CMAKE_CXX-tm_LINK_DIRS64_REGEX "/lib/intel64")
set(CMAKE_CXX-tm_VERBOSE_FLAG "-v")

set(CMAKE_CXX-tm_FLAGS_INIT "-Qtm_enabled")
set(CMAKE_CXX-tm_FLAGS_DEBUG_INIT "-g")
set(CMAKE_CXX-tm_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")
set(CMAKE_CXX-tm_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_CXX-tm_FLAGS_RELWITHDEBINFO_INIT "-O3 -g")
set(CMAKE_CXX-tm_COMPILER_ARG1 "-Qtm_enabled")

set(CMAKE_CXX-tm_CREATE_PREPROCESSED_SOURCE
  "<CMAKE_CXX-tm_COMPILER> <DEFINES> <FLAGS> -E <SOURCE> > <PREPROCESSED_SOURCE>")
set(CMAKE_CXX-tm_CREATE_ASSEMBLY_SOURCE
  "<CMAKE_CXX-tm_COMPILER> <DEFINES> <FLAGS> -S <SOURCE> -o <ASSEMBLY_SOURCE>")
