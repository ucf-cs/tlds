#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

set(
  sources
  ../lib/avltree.c
  ../lib/heap.c
  ../lib/list.c
  ../lib/mt19937ar.c
  ../lib/pair.c
  ../lib/queue.c
  ../lib/random.c
  ../lib/rbtree.c
  ../lib/thread.c
  ../lib/vector.c
  coordinate.c
  element.c
  mesh.c
  region.c
  yada.c)

# build the different executables
foreach (arch ${rstm_archs})
  add_stamp_executable(exec yada ${arch} ${sources})
  add_target_definitions(${exec} LIST_NO_DUPLICATES MAP_USE_AVLTREE SET_USE_RBTREE)
endforeach ()
