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
  alg_radix_smp.c
  computeGraph.c
  createPartition.c
  cutClusters.c
  findSubGraphs.c
  genScalData.c
  getStartLists.c
  getUserParameters.c
  globals.c
  ssca2.c)

# build the different executables
foreach (arch ${rstm_archs})
  add_stamp_executable(exec ssca2 ${arch} ${sources})
  add_target_definitions(${exec} ENABLE_KERNEL1
    #USE_PARALLEL_DATA_GENERATION
    #WRITE_RESULT_FILES
    #ENABLE_KERNEL2
    #ENABLE_KERNEL3
    #ENABLE_KERNEL4
    )

  if (rstm_enable_itm)
    add_stamp_itm_executable(exec_itm ssca2 ${arch} ${sources})
    add_target_definitions(${exec_itm} ENABLE_KERNEL1)
  endif ()
  
  if (rstm_enable_itm2stm)
    add_stamp_itm2stm_executable(exec_shim ssca2 ${arch} ${sources})
    add_target_definitions(${exec_shim} ENABLE_KERNEL1)
  endif ()
endforeach ()
