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
  ../lib/pair.c
  ../lib/mt19937ar.c
  ../lib/random.c
  ../lib/thread.c
  client.c
  customer.c
  manager.c
  reservation.c
  vacation.c
  ../lib/list.c
  ../lib/rbtree.c)

# build the different executables
foreach (arch ${rstm_archs})
  add_stamp_executable(exec vacation ${arch} ${sources})
  add_target_definitions(${exec} LIST_NO_DUPLICATES MAP_USE_RBTREE)
    if (rstm_enable_itm)
    add_stamp_itm_executable(exec_itm vacation ${arch} ${sources})
    add_target_definitions(${exec_itm} LIST_NO_DUPLICATES MAP_USE_RBTREE)
  endif ()
  if (rstm_enable_itm2stm)
    add_stamp_itm2stm_executable(exec_shim vacation ${arch} ${sources})
    add_target_definitions(${exec_shim} LIST_NO_DUPLICATES MAP_USE_RBTREE)
  endif ()
endforeach ()
