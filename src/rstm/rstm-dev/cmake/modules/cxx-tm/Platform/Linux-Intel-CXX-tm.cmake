#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

INCLUDE(Platform/Linux-Intel)
set(CMAKE_CXX-tm_EXTRA_LIBS -lirc)
__linux_compiler_intel(CXX-tm)
