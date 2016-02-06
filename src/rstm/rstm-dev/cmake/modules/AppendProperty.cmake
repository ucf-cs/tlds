#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information


## This helper macro adds to a property.  What that means is that, for
##  example, we can say 'for SOURCE=bytelazy.cpp, the CXXFLAGS should have
## '-fno-strict-aliasing' appended to it

## generally, this seems pretty dangerous, because it gives a back-door for
## changing the CXXFLAGS (among other things) on a per-file basis.  It is
## nice, though, in that it lets us turn off a specific warning for a
## specific file, without having that warning off all the time.

macro (append_property type var prop)
  get_property (old ${type} ${var} PROPERTY ${prop})
  foreach (val ${ARGN})
    if (NOT old MATCHES ${val})
      set (old "${old} ${val}")
    endif ()
  endforeach (val)
  set_property (${type} ${var} PROPERTY ${prop} ${old})
endmacro (append_property)

# Some properties (like OBJECT_DEPENDS) are actually lists, so we need a ; when
# appending. 
macro (append_list_property type var prop)
  get_property (old ${type} ${var} PROPERTY ${prop})
  foreach (val ${ARGN})
    if (NOT old MATCHES ${val})
      set (old "${old};${val}")
    endif ()
  endforeach (val)
  set_property (${type} ${var} PROPERTY ${prop} ${old})
endmacro ()
