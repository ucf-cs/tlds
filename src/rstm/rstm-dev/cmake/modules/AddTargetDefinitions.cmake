#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

# Cmake comes with a builtin add_definitions command to add definitions to
# everything in the current directory, but we need to add definitions to
# targets. 
function (add_target_definitions target)
  get_target_property(defs ${target} COMPILE_DEFINITIONS)
  if (defs MATCHES "NOTFOUND")
    set(defs "")
  endif ()
  foreach (def ${defs} ${ARGN})
    list(APPEND deflist ${def})
  endforeach ()
  set_target_properties(${target} PROPERTIES COMPILE_DEFINITIONS "${deflist}")
endfunction ()
