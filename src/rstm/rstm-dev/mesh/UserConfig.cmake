#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

option(
  mesh_enable_fgl
  "ON to build the fine-grain-locking mesh implementation (meshFGL)." ON)

option(
  mesh_enable_cgl
  "ON to build the coarse-grain locking mesh implementation (meshCGL)." ON)

set(
  mesh_libstdcxx-v3_include "" CACHE PATH
  "Path to icpc compatible libstdc++-v3 include dir (<gcc 4.3.X src>/stdc++-v3/include)")
