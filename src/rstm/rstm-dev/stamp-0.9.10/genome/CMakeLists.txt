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
  ../lib/hash.c
  ../lib/hashtable.c
  ../lib/pair.c
  ../lib/random.c
  ../lib/list.c
  ../lib/mt19937ar.c
  ../lib/thread.c
  ../lib/vector.c
  gene.c
  genome.c
  segments.c
  sequencer.c
  table.c)

# Force the use of the C++ compiler
set_source_files_properties(${sources} PROPERTIES LANGUAGE CXX)

# build the different executables
foreach (arch ${rstm_archs})
  add_stamp_executable(exec genome ${arch} ${sources})
  add_target_definitions(${exec} LIST_NO_DUPLICATES CHUNK_STEP1=12)
endforeach ()
