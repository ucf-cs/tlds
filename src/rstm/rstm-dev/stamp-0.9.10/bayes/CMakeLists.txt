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
  ../lib/bitmap.c
  ../lib/list.c
  ../lib/mt19937ar.c
  ../lib/queue.c
  ../lib/random.c
  ../lib/thread.c
  ../lib/vector.c
  adtree.c
  bayes.c
  data.c
  learner.c
  net.c
  sort.c)

# build the different executables
foreach (arch ${rstm_archs})
  add_stamp_executable(exec bayes ${arch} ${sources})
  add_target_definitions(${exec}
    LIST_NO_DUPLICATES LEARNER_TRY_REMOVE LEARNER_TRY_REVERSE) 
endforeach ()
