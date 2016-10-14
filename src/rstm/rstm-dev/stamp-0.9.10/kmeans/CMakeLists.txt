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
  ../lib/mt19937ar.c
  ../lib/random.c
  ../lib/thread.c
  cluster.c
  common.c
  kmeans.c
  normal.c)

# build the different executables
foreach (arch ${rstm_archs})
  add_stamp_executable(exec kmeans ${arch} ${sources})
  add_target_definitions(${exec} OUTPUT_TO_STDOUT)
  if (rstm_enable_itm)
    add_stamp_itm_executable(exec_itm kmeans ${arch} ${sources})
    add_target_definitions(${exec_itm} OUTPUT_TO_STDOUT)
  endif ()
  if (rstm_enable_itm2stm)
    add_stamp_itm2stm_executable(exec_shim kmeans ${arch} ${sources})
    add_target_definitions(${exec_shim} OUTPUT_TO_STDOUT)
  endif ()
endforeach ()
