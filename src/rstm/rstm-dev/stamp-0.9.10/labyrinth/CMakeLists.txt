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
  ../lib/list.c
  ../lib/mt19937ar.c
  ../lib/pair.c
  ../lib/queue.c
  ../lib/random.c
  ../lib/thread.c
  ../lib/vector.c
  coordinate.c
  grid.c
  labyrinth.c
  maze.c
  router.c)

# build the different executables
foreach (arch ${rstm_archs})
  add_stamp_executable(exec labyrinth ${arch} ${sources})
  target_link_libraries(${exec} -lm)
  add_target_definitions(${exec} USE_EARLY_RELEASE)
endforeach ()
